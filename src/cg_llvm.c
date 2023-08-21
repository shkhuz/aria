#include "cg.h"
#include "ast.h"
#include "type.h"
#include "buf.h"
#include "compile.h"

CgCtx cg_new_context(struct Typespec** mod_tys, struct CompileCtx* compile_ctx) {
    CgCtx c;
    c.mod_tys = mod_tys;
    c.current_mod_ty = NULL;
    c.current_func = NULL;
    c.compile_ctx = compile_ctx;
    c.error = false;
    return c;
}

static inline void msg_emit(CgCtx* c, Msg* msg) {
    _msg_emit(msg, c->compile_ctx);
    if (msg->kind == MSG_ERROR) {
        c->error = true;
    }
}

static char* mangle_name(CgCtx* c, char* name) {
    return format_string(
        "_Z%lu%s",
        c->current_mod_ty->mod.srcfile->id,
        name);
}

static LLVMTypeRef cg_get_llvm_type(Typespec* typespec) {
    if (typespec->llvmtype) return typespec->llvmtype;

    switch (typespec->kind) {
        case TS_PRIM: {
            switch (typespec->prim.kind) {
                case PRIM_u8:
                case PRIM_u16:
                case PRIM_u32:
                case PRIM_u64:
                case PRIM_i8:
                case PRIM_i16:
                case PRIM_i32:
                case PRIM_i64:
                    typespec->llvmtype = LLVMIntType(typespec_get_bytes(typespec) * 8);
                    break;

                case PRIM_bool:
                    typespec->llvmtype = LLVMInt8Type();
                    break;

                case PRIM_INTEGER:
                    typespec->llvmtype = LLVMInt64Type();
                    break;

                default: assert(0 && "cg_get_llvm_type()");
            }
        } break;

        case TS_void:
        case TS_noreturn:
            typespec->llvmtype = LLVMVoidType();
            break;

        case TS_PTR:
            typespec->llvmtype = LLVMPointerType(cg_get_llvm_type(typespec->ptr.child), 0);
            break;

        case TS_MULTIPTR:
            typespec->llvmtype = LLVMPointerType(cg_get_llvm_type(typespec->mulptr.child), 0);
            break;

        case TS_FUNC: {
            LLVMTypeRef* param_llvmtypes = NULL;
            Typespec** params = typespec->func.params;
            Typespec* ret = typespec->func.ret_typespec;
            bufloop(params, i) {
                bufpush(param_llvmtypes, cg_get_llvm_type(params[i]));
            }
            LLVMTypeRef ret_llvmtype = cg_get_llvm_type(ret);
            typespec->llvmtype = LLVMFunctionType(
                ret_llvmtype,
                param_llvmtypes,
                buflen(params),
                false);
        };

        case TS_STRUCT:
            typespec->llvmtype = typespec->agg->strct.llvmtype;
            break;

        default: assert(0 && "cg_get_llvm_type()");
    }
    return typespec->llvmtype;
}

static void cg_top_level_decls_prec1(CgCtx* c, AstNode* astnode) {
    switch (astnode->kind) {
        case ASTNODE_STRUCT: {
            astnode->strct.llvmtype = LLVMStructCreateNamed(
                LLVMGetGlobalContext(),
                token_tostring(astnode->strct.identifier));
        } break;
    }
}

static void cg_function_header(CgCtx* c, AstNode* header, bool should_mangle) {
    header->funch.mangled_name = should_mangle ? mangle_name(c, header->funch.name) : header->funch.name;
    LLVMTypeRef* param_llvmtypes = NULL;
    bufloop(header->funch.params, i) {
        cg_get_llvm_type(header->funch.params[i]->typespec);
        bufpush(param_llvmtypes, header->funch.params[i]->typespec->llvmtype);
    }
    LLVMTypeRef ret_llvmtype = cg_get_llvm_type(header->typespec->func.ret_typespec);
    header->funch.ret_typespec->typespec->llvmtype = ret_llvmtype;

    header->typespec->llvmtype = LLVMFunctionType(
        ret_llvmtype,
        param_llvmtypes,
        buflen(header->funch.params),
        false);
}

static void cg_top_level_decls_prec2(CgCtx* c, AstNode* astnode) {
    switch (astnode->kind) {
        case ASTNODE_VARIABLE_DECL: {
            cg_get_llvm_type(astnode->typespec);
            astnode->llvmvalue = LLVMAddGlobal(
                c->llvmmod,
                astnode->typespec->llvmtype,
                token_tostring(astnode->vard.identifier));
        } break;

        case ASTNODE_FUNCTION_DEF: {
            cg_function_header(c, astnode->funcdef.header, !astnode->funcdef.export);
            astnode->llvmvalue = LLVMAddFunction(
                c->llvmmod,
                astnode->funcdef.header->funch.mangled_name,
                astnode->typespec->llvmtype);
        } break;

        case ASTNODE_EXTERN_FUNCTION: {
            cg_function_header(c, astnode->extfunc.header, false);
            astnode->llvmvalue = LLVMAddFunction(
                c->llvmmod,
                astnode->extfunc.header->funch.mangled_name,
                astnode->typespec->llvmtype);
        } break;

        case ASTNODE_STRUCT: {
            LLVMTypeRef* field_llvmtypes = NULL;
            bufloop(astnode->strct.fields, i) {
                cg_get_llvm_type(astnode->strct.fields[i]->typespec);
                bufpush(field_llvmtypes, astnode->strct.fields[i]->typespec->llvmtype);
            }

            LLVMStructSetBody(
                astnode->strct.llvmtype,
                field_llvmtypes,
                buflen(field_llvmtypes),
                false);
        } break;
    }
}

LLVMValueRef cg_astnode(CgCtx* c, AstNode* astnode, bool lvalue, Typespec* target);

typedef struct {
    LLVMBasicBlockRef condbb;
    LLVMBasicBlockRef bodybb;
} CondAndBodyBB;

LLVMValueRef cg_astnode(CgCtx* c, AstNode* astnode, bool lvalue, Typespec* target) {
    assert(astnode->typespec);
    if (typespec_is_unsized_integer(astnode->typespec) && target) {
        astnode->llvmvalue = LLVMConstInt(
                typespec_is_sized_integer(target) ? cg_get_llvm_type(target) : LLVMInt64Type(),
                astnode->typespec->prim.integer.neg ? (-astnode->typespec->prim.integer.d[0]) : astnode->typespec->prim.integer.d[0],
                astnode->typespec->prim.integer.neg);
        return astnode->llvmvalue;
    }

    switch (astnode->kind) {
        case ASTNODE_INTEGER_LITERAL: {
//            astnode->llvmvalue = LLVMConstInt(
//                    LLVMIntType(64),
//                    astnode->intl.val.neg ? (-astnode->intl.val.d[0]) : astnode->intl.val.d[0],
//                    astnode->intl.val.neg);
        } break;

        case ASTNODE_FUNCTION_CALL: {
            LLVMValueRef callee_llvmvalue = cg_astnode(c, astnode->funcc.callee, false, NULL);
            Typespec* func_ty = astnode->funcc.callee->typespec->kind == TS_PTR
                ? astnode->funcc.callee->typespec->ptr.child
                : astnode->funcc.ref->funcdef.header->typespec;

            LLVMValueRef* arg_llvmvalues = NULL;
            if (buflen(astnode->funcc.args) != 0) {
                bufloop(astnode->funcc.args, i) {
                    bufpush(arg_llvmvalues, cg_astnode(
                        c,
                        astnode->funcc.args[i],
                        false,
                        func_ty->func.params[i]));
                }
            }

            astnode->typespec->llvmtype = cg_get_llvm_type(func_ty->func.ret_typespec);
            astnode->llvmvalue = LLVMBuildCall2(
                c->llvmbuilder,
                func_ty->llvmtype,
                callee_llvmvalue,
                arg_llvmvalues,
                buflen(astnode->funcc.args),
                "");
        } break;

        case ASTNODE_SYMBOL: {
            astnode->typespec->llvmtype = astnode->sym.ref->typespec->llvmtype;

            astnode->llvmvalue = lvalue || astnode->sym.ref->kind == ASTNODE_FUNCTION_DEF || astnode->sym.ref->kind == ASTNODE_EXTERN_FUNCTION
                ? astnode->sym.ref->llvmvalue
                : LLVMBuildLoad2(c->llvmbuilder, astnode->sym.ref->typespec->llvmtype, astnode->sym.ref->llvmvalue, "");
        } break;

        case ASTNODE_BUILTIN_SYMBOL: {
            switch (astnode->bsym.kind) {
                case BS_true: {
                    cg_get_llvm_type(astnode->typespec);
                    astnode->llvmvalue = LLVMConstInt(LLVMInt8Type(), 1, false);
                } break;

                case BS_false: {
                    cg_get_llvm_type(astnode->typespec);
                    astnode->llvmvalue = LLVMConstInt(LLVMInt8Type(), 0, false);
                } break;
            }
        } break;

        case ASTNODE_CAST: {
            astnode->llvmvalue = cg_astnode(c, astnode->cast.left, false, astnode->cast.right->typespec->ty);

            Typespec* left = astnode->cast.left->typespec;
            assert(astnode->cast.right->typespec->kind == TS_TYPE);
            Typespec* right = astnode->cast.right->typespec->ty;

            if (left->kind == TS_PRIM && right->kind == TS_PRIM && left->prim.kind != right->prim.kind) {
                astnode->llvmvalue = LLVMBuildIntCast2(
                        c->llvmbuilder,
                        astnode->llvmvalue,
                        cg_get_llvm_type(right),
                        // example case: i16 to u64
                        // 1) upcast i16 to i64
                        // 2) now as the sizes are same, converting is just
                        //    a matter of observation :)
                        // therefore, when casting to target type,
                        // always sign or zero extend from the src type
                        typespec_is_unsized_integer(left)
                                ? typespec_is_signed(right)
                                : typespec_is_signed(left),
                        "");
            } else if (left->kind == TS_PRIM && (right->kind == TS_PTR || right->kind == TS_MULTIPTR)) {
                astnode->llvmvalue = LLVMBuildIntToPtr(
                        c->llvmbuilder,
                        astnode->llvmvalue,
                        cg_get_llvm_type(right),
                        "");
            } else if ((left->kind == TS_PTR || left->kind == TS_MULTIPTR) && right->kind == TS_PRIM) {
                astnode->llvmvalue = LLVMBuildPtrToInt(
                        c->llvmbuilder,
                        astnode->llvmvalue,
                        cg_get_llvm_type(right),
                        "");
            }
        } break;

        case ASTNODE_ASSIGN: {
            Typespec* left = astnode->assign.left->typespec;
            Typespec* right = astnode->assign.right->typespec;
            Typespec* left_target_type = NULL;
            Typespec* right_target_type = NULL;
            if (typespec_is_unsized_integer(right))
                right_target_type = left;
            else if (typespec_get_bytes(left) > typespec_get_bytes(right))
                right_target_type = left;
            else
                left_target_type = right;
            LLVMValueRef left_llvmvalue = cg_astnode(
                c,
                astnode->assign.left,
                true,
                left_target_type);
            LLVMValueRef right_llvmvalue = cg_astnode(
                c,
                astnode->assign.right,
                false,
                right_target_type);
            LLVMBuildStore(c->llvmbuilder, right_llvmvalue, left_llvmvalue);
        } break;

        case ASTNODE_ARITH_BINOP: {
            cg_get_llvm_type(astnode->typespec);
            LLVMValueRef left = cg_astnode(c, astnode->arthbin.left, false, astnode->typespec);
            LLVMValueRef right = cg_astnode(c, astnode->arthbin.right, false, astnode->typespec);
            LLVMOpcode op;
            switch (astnode->arthbin.kind) {
                case ARITH_BINOP_ADD: op = LLVMAdd; break;
                case ARITH_BINOP_SUB: op = LLVMSub; break;
                case ARITH_BINOP_MUL: op = LLVMMul; break;
                case ARITH_BINOP_DIV: {
                    if (typespec_is_signed(astnode->typespec))
                        op = LLVMSDiv;
                    else
                        op = LLVMUDiv;
                } break;
                default: assert(0); break;
            }
            astnode->llvmvalue = LLVMBuildBinOp(c->llvmbuilder, op, left, right, "");
        } break;

        case ASTNODE_CMP_BINOP: {
            cg_get_llvm_type(astnode->typespec);
            cg_get_llvm_type(astnode->cmpbin.peerres);
            LLVMValueRef left = cg_astnode(c, astnode->cmpbin.left, false, astnode->cmpbin.peerres);
            LLVMValueRef right = cg_astnode(c, astnode->cmpbin.right, false, astnode->cmpbin.peerres);
            LLVMIntPredicate op;
            bool signd;
            if (typespec_is_integer(astnode->cmpbin.left->typespec)) {
                signd = typespec_is_signed(astnode->cmpbin.left->typespec);
            }

            switch (astnode->cmpbin.kind) {
                case CMP_BINOP_EQ: op = LLVMIntEQ; break;
                case CMP_BINOP_NE: op = LLVMIntNE; break;
                case CMP_BINOP_LT: signd ? op = LLVMIntSLT : (op = LLVMIntULT); break;
                case CMP_BINOP_GT: signd ? op = LLVMIntSGT : (op = LLVMIntUGT); break;
                case CMP_BINOP_LE: signd ? op = LLVMIntSLE : (op = LLVMIntULE); break;
                case CMP_BINOP_GE: signd ? op = LLVMIntSGE : (op = LLVMIntUGE); break;
            }
            astnode->llvmvalue = LLVMBuildICmp(
                    c->llvmbuilder,
                    op,
                    left,
                    right,
                    "");
        } break;

        case ASTNODE_UNOP: {
            cg_get_llvm_type(astnode->typespec);
            switch (astnode->unop.kind) {
                case UNOP_NEG: {
                    LLVMValueRef child_llvmvalue = cg_astnode(c, astnode->unop.child, false, astnode->typespec);
                    astnode->llvmvalue = LLVMBuildNeg(c->llvmbuilder, child_llvmvalue, "");
                } break;

                case UNOP_BOOLNOT: {} break;

                case UNOP_ADDR: {
                    astnode->llvmvalue = cg_astnode(c, astnode->unop.child, true, NULL);
                } break;
            }
        } break;

        case ASTNODE_DEREF: {
            cg_get_llvm_type(astnode->typespec);
            LLVMValueRef child_llvmvalue = cg_astnode(c, astnode->deref.child, false, NULL);
            if (lvalue)
                astnode->llvmvalue = child_llvmvalue;
            else
                astnode->llvmvalue = LLVMBuildLoad2(
                    c->llvmbuilder,
                    astnode->typespec->llvmtype,
                    child_llvmvalue,
                    "");
        } break;

        case ASTNODE_ACCESS: {
            astnode->typespec->llvmtype = astnode->acc.accessed->typespec->llvmtype;
            astnode->llvmvalue = astnode->acc.accessed->llvmvalue;
        } break;

        case ASTNODE_EXPRSTMT: {
            astnode->llvmvalue = cg_astnode(c, astnode->exprstmt, false, NULL);
        } break;

        case ASTNODE_VARIABLE_DECL: {
            if (astnode->vard.initializer && !typespec_is_comptime(astnode->typespec)) {
                LLVMValueRef initializer_llvmvalue = cg_astnode(c, astnode->vard.initializer, false, astnode->typespec);
                LLVMBuildStore(c->llvmbuilder, initializer_llvmvalue, astnode->llvmvalue);
            }
        } break;

        case ASTNODE_SCOPED_BLOCK: {
            bufloop(astnode->blk.stmts, i) {
                cg_astnode(c, astnode->blk.stmts[i], false, NULL);
            }
        } break;

        case ASTNODE_IF_BRANCH: {
            if (astnode->ifbr.kind != IFBR_ELSE) {
                if (c->ifstate.brcondbb) LLVMPositionBuilderAtEnd(c->llvmbuilder, c->ifstate.brcondbb);
                LLVMValueRef cond_llvmvalue = cg_astnode(c, astnode->ifbr.cond, false, NULL);
                LLVMBuildCondBr(
                        c->llvmbuilder,
                        LLVMBuildIntCast2(
                                c->llvmbuilder,
                                cond_llvmvalue,
                                LLVMInt1Type(),
                                false,
                                ""),
                        c->ifstate.brbodybb,
                        c->ifstate.nextbb);
            }

            LLVMPositionBuilderAtEnd(c->llvmbuilder, c->ifstate.brbodybb);
            astnode->llvmvalue = cg_astnode(c, astnode->ifbr.body, lvalue, target);
            cg_get_llvm_type(astnode->typespec);
            if (astnode->typespec->kind != TS_noreturn)
                LLVMBuildBr(c->llvmbuilder, c->ifstate.endifexprbb);
            LLVMPositionBuilderAtEnd(c->llvmbuilder, c->ifstate.nextbb);
        } break;

        case ASTNODE_IF: {
            LLVMBasicBlockRef ifbrbb = LLVMAppendBasicBlock(
                    c->current_func->llvmvalue,
                    "if.body");
            CondAndBodyBB* elseifbrbb = NULL;
            bufloop(astnode->iff.elseifbr, i) {
                bufpush(elseifbrbb, (CondAndBodyBB){
                    LLVMAppendBasicBlock(
                            c->current_func->llvmvalue,
                            "elseif.cond"),
                    LLVMAppendBasicBlock(
                            c->current_func->llvmvalue,
                            "elseif.body")
                });
            }
            LLVMBasicBlockRef elsebrbb = NULL;
            if (astnode->iff.elsebr) {
                elsebrbb = LLVMAppendBasicBlock(
                        c->current_func->llvmvalue,
                        "else.body");
            }
            LLVMBasicBlockRef endifexprbb = LLVMAppendBasicBlock(
                    c->current_func->llvmvalue,
                    "if.end");
            c->ifstate.endifexprbb = endifexprbb;

            c->ifstate.brcondbb = NULL;
            c->ifstate.brbodybb = ifbrbb;
            c->ifstate.nextbb = buflen(elseifbrbb) != 0
                    ? elseifbrbb[0].condbb
                    : (elsebrbb
                        ? elsebrbb
                        : endifexprbb);
            LLVMValueRef ifbrval = cg_astnode(
                    c,
                    astnode->iff.ifbr,
                    lvalue,
                    astnode->typespec);

            LLVMValueRef* elseifbrval = NULL;
            bufloop(astnode->iff.elseifbr, i) {
                c->ifstate.brcondbb = elseifbrbb[i].condbb;
                c->ifstate.brbodybb = elseifbrbb[i].bodybb;
                c->ifstate.nextbb = i < buflen(elseifbrbb)-1
                        ? elseifbrbb[i+1].condbb
                        : (elsebrbb
                            ? elsebrbb
                            : endifexprbb);
                bufpush(elseifbrval, cg_astnode(
                        c,
                        astnode->iff.elseifbr[i],
                        lvalue,
                        astnode->typespec));
            }

            LLVMValueRef elsebrval = NULL;
            if (astnode->iff.elsebr) {
                c->ifstate.brcondbb = NULL;
                c->ifstate.brbodybb = elsebrbb;
                c->ifstate.nextbb = endifexprbb;
                elsebrval = cg_astnode(
                        c,
                        astnode->iff.elsebr,
                        lvalue,
                        astnode->typespec);
            }

            if (astnode->typespec->kind != TS_void
                && astnode->typespec->kind != TS_noreturn) {
                LLVMValueRef phi = LLVMBuildPhi(
                        c->llvmbuilder,
                        cg_get_llvm_type(astnode->typespec),
                        "");
                if (astnode->iff.ifbr->typespec->kind != TS_noreturn)
                    LLVMAddIncoming(phi, &ifbrval, &ifbrbb, 1);
                bufloop(astnode->iff.elseifbr, i) {
                    if (astnode->iff.elseifbr[i]->typespec->kind != TS_noreturn)
                        LLVMAddIncoming(phi, &elseifbrval[i], &elseifbrbb[i].bodybb, 1);
                }
                if (elsebrval && astnode->iff.elsebr->typespec->kind != TS_noreturn) {
                    LLVMAddIncoming(phi, &elsebrval, &elsebrbb, 1);
                }
                astnode->llvmvalue = phi;
            }
        } break;

        case ASTNODE_RETURN: {
            if (astnode->ret.child) {
                LLVMValueRef child_llvmvalue = cg_astnode(
                    c,
                    astnode->ret.child,
                    false,
                    astnode->ret.ref->typespec->func.ret_typespec);
                astnode->llvmvalue = LLVMBuildRet(c->llvmbuilder, child_llvmvalue);
            } else {
                astnode->llvmvalue = LLVMBuildRetVoid(c->llvmbuilder);
            }
        } break;

        case ASTNODE_FUNCTION_DEF: {
            c->current_func = astnode;
            AstNodeFunctionHeader* header = &astnode->funcdef.header->funch;
            LLVMBasicBlockRef entry = LLVMAppendBasicBlock(astnode->llvmvalue, "entry");
            LLVMPositionBuilderAtEnd(c->llvmbuilder, entry);

            AstNode** params = header->params;
            usize params_len = buflen(params);
            LLVMValueRef* param_llvmvalues = NULL;
            if (params_len != 0) {
                param_llvmvalues = (LLVMValueRef*)malloc(sizeof(LLVMValueRef) * params_len);
                LLVMGetParams(astnode->llvmvalue, param_llvmvalues);
                for (usize i = 0; i < params_len; i++) {
                    LLVMSetValueName2(
                        param_llvmvalues[i],
                        params[i]->paramd.name,
                        params[i]->paramd.identifier->span.end - params[i]->paramd.identifier->span.start);
                    params[i]->llvmvalue = LLVMBuildAlloca(c->llvmbuilder, params[i]->typespec->llvmtype, strcat(params[i]->paramd.name, ".addr"));
                    LLVMBuildStore(c->llvmbuilder, param_llvmvalues[i], params[i]->llvmvalue);
                }
            }

            AstNode** locals = astnode->funcdef.locals;
            bufloop(locals, i) {
                if (!typespec_is_comptime(locals[i]->typespec)) {
                    cg_get_llvm_type(locals[i]->typespec);
                    locals[i]->llvmvalue = LLVMBuildAlloca(
                        c->llvmbuilder,
                        locals[i]->typespec->llvmtype,
                        locals[i]->vard.name);
                }
            }

            LLVMValueRef body_llvmvalue = cg_astnode(c, astnode->funcdef.body, false, NULL);
            AstNode* last_stmt = astnode->funcdef.body->blk.stmts
                    ? astnode->funcdef.body->blk.stmts[buflen(astnode->funcdef.body->blk.stmts)-1]
                    : NULL;
            if (astnode->typespec->func.ret_typespec->kind == TS_void
                && (!last_stmt || (last_stmt->kind != ASTNODE_EXPRSTMT || last_stmt->exprstmt->kind != ASTNODE_RETURN))) {
                LLVMBuildRetVoid(c->llvmbuilder);
            }
            if (astnode->typespec->func.ret_typespec->kind == TS_noreturn) {
                LLVMBuildUnreachable(c->llvmbuilder);
            }
            c->current_func = NULL;
        } break;

        case ASTNODE_EXTERN_FUNCTION: {} break;

        case ASTNODE_STRUCT: {} break;
        case ASTNODE_IMPORT: {} break;

        default: assert(0); break;
    }

    // Integer implicit casting
    if (target
        && typespec_is_sized_integer(target)
        && typespec_is_sized_integer(astnode->typespec)
        && typespec_is_signed(target) == typespec_is_signed(astnode->typespec)) {
        if (typespec_get_bytes(target) > typespec_get_bytes(astnode->typespec)) {
            if (typespec_is_signed(target)) {
                astnode->llvmvalue = LLVMBuildSExt(
                    c->llvmbuilder,
                    astnode->llvmvalue,
                    cg_get_llvm_type(target),
                    "");
            } else {
                astnode->llvmvalue = LLVMBuildZExt(
                    c->llvmbuilder,
                    astnode->llvmvalue,
                    cg_get_llvm_type(target),
                    "");
            }
        }
    }

    return astnode->llvmvalue;
}

static bool init_cg(CgCtx* c) {
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    if (!c->compile_ctx->target_triple) {
        c->compile_ctx->target_triple = LLVMGetDefaultTargetTriple();
    }

    char* errors = NULL;
    bool error = LLVMGetTargetFromTriple(
        c->compile_ctx->target_triple,
        &c->llvmtarget,
        &errors);
    if (error) {
        Msg msg = msg_with_no_span(
            MSG_ERROR,
            format_string("invalid or unsupported target: '%s'", c->compile_ctx->target_triple));
        msg_addl_thin(&msg, errors);
        msg_emit(c, &msg);
    }
    LLVMDisposeMessage(errors);
    errors = NULL;
    if (error) return true;

    //printf(
    //        "======== MACHINE INFO ========\n"
    //        "target: %s, [%s], %d, %d\n",
    //        LLVMGetTargetName(c->llvmtarget),
    //        LLVMGetTargetDescription(c->llvmtarget),
    //        LLVMTargetHasJIT(c->llvmtarget),
    //        LLVMTargetHasTargetMachine(c->llvmtarget));
    //printf("host triple: %s\n", LLVMGetDefaultTargetTriple());

    c->llvmtargetmachine = LLVMCreateTargetMachine(
            c->llvmtarget,
            c->compile_ctx->target_triple,
            "generic",
            "",
            LLVMCodeGenLevelDefault,
            LLVMRelocDefault,
            LLVMCodeModelDefault);
    c->llvmtargetdatalayout = LLVMCreateTargetDataLayout(c->llvmtargetmachine);

    c->llvmpm = LLVMCreatePassManager();
    LLVMAddPromoteMemoryToRegisterPass(c->llvmpm);

    return false;
}

bool cg(CgCtx* c) {
    if (init_cg(c)) return true;

    c->llvmbuilder = LLVMCreateBuilder();
    c->llvmmod = LLVMModuleCreateWithName("root");

    bufloop(c->mod_tys, i) {
        Srcfile* srcfile = c->mod_tys[i]->mod.srcfile;
        c->current_mod_ty = c->mod_tys[i];
        bufloop(srcfile->astnodes, j) {
            cg_top_level_decls_prec1(c, srcfile->astnodes[j]);
        }
    }

    bufloop(c->mod_tys, i) {
        Srcfile* srcfile = c->mod_tys[i]->mod.srcfile;
        c->current_mod_ty = c->mod_tys[i];
        bufloop(srcfile->astnodes, j) {
            cg_top_level_decls_prec2(c, srcfile->astnodes[j]);
        }
    }

    bufloop(c->mod_tys, i) {
        Srcfile* srcfile = c->mod_tys[i]->mod.srcfile;
        c->current_mod_ty = c->mod_tys[i];
        bufloop(srcfile->astnodes, j) {
            cg_astnode(c, srcfile->astnodes[j], false, NULL);
        }
    }

    printf("\n======= Raw LLVM IR Start =======\n");
    LLVMDumpModule(c->llvmmod);
    printf("\n======= Raw LLVM IR End =======\n");

    bool error = false;
    char* errors = NULL;

    error = LLVMVerifyModule(c->llvmmod, LLVMReturnStatusAction, &errors);
    if (error) {
        Msg msg = msg_with_no_span(
            MSG_ERROR,
            "[internal] IR generated by the compiler is invalid");
        msg_addl_thin(&msg, errors);
        msg_emit(c, &msg);
    }

    LLVMDisposeMessage(errors);
    errors = NULL;
    if (error) {
        c->error = true;
        return c->error;
    }

    // NOTE: I've disabled memory optimizations for now, because LLVM
    // deletes all unused locals, and I need them to form a memory buffer
    // in the examples/nostd program.
    //LLVMRunPassManager(c->llvmpm, c->llvmmod);

    printf("\n======= Optimized LLVM IR Start =======\n");
    LLVMDumpModule(c->llvmmod);
    printf("\n======= Optimized LLVM IR End =======\n");

    LLVMMemoryBufferRef objbuf = NULL;
    LLVMTargetMachineEmitToMemoryBuffer(
        c->llvmtargetmachine,
        c->llvmmod,
        LLVMObjectFile,
        NULL,
        &objbuf);
    write_bin_file("mod.o", LLVMGetBufferStart(objbuf), LLVMGetBufferSize(objbuf));
    LLVMDisposeMemoryBuffer(objbuf);

    return c->error;
}
