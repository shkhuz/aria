#include "cg.h"
#include "ast.h"
#include "type.h"
#include "buf.h"
#include "compile.h"

CgCtx cg_new_context(struct Typespec** mod_tys, struct CompileCtx* compile_ctx) {
    CgCtx c;
    c.mod_tys = mod_tys;
    c.current_mod_ty = NULL;
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

                case PRIM_void:
                    typespec->llvmtype = LLVMVoidType();
                    break;

                case PRIM_integer:
                    typespec->llvmtype = LLVMInt64Type();
                    break;

                default: assert(0 && "cg_get_llvm_type()");
            }
        } break;

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

        case TS_NORETURN:
            typespec->llvmtype = LLVMVoidType();
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
            AstNodeFunctionHeader* header = &astnode->funcdef.header->funch;
            header->mangled_name = astnode->funcdef.export ? header->name : mangle_name(c, header->name);
            LLVMTypeRef* param_llvmtypes = NULL;
            bufloop(header->params, i) {
                cg_get_llvm_type(header->params[i]->typespec);
                bufpush(param_llvmtypes, header->params[i]->typespec->llvmtype);
            }
            LLVMTypeRef ret_llvmtype = cg_get_llvm_type(astnode->typespec->func.ret_typespec);
            astnode->funcdef.header->funch.ret_typespec->typespec->llvmtype = ret_llvmtype;

            astnode->funcdef.header->typespec->llvmtype = LLVMFunctionType(
                ret_llvmtype,
                param_llvmtypes,
                buflen(header->params),
                false);
            astnode->llvmvalue = LLVMAddFunction(
                c->llvmmod,
                header->mangled_name,
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

LLVMValueRef cg_astnode(CgCtx* c, AstNode* astnode, bool lvalue, Typespec* target) {
    assert(astnode->typespec);
    LLVMValueRef result = NULL;
    if (typespec_is_unsized_integer(astnode->typespec) && target) {
        astnode->llvmvalue = LLVMConstInt(
            typespec_is_sized_integer(target) ? cg_get_llvm_type(target) : LLVMInt64Type(),
            astnode->typespec->prim.integer.neg ? (-astnode->typespec->prim.integer.d[0]) : astnode->typespec->prim.integer.d[0],
            astnode->typespec->prim.integer.neg);
        return astnode->llvmvalue;
    }

    switch (astnode->kind) {
        // Integer literal case handled above
        case ASTNODE_INTEGER_LITERAL: assert(0); break;

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
            result = astnode->llvmvalue;
        } break;

        case ASTNODE_SYMBOL: {
            astnode->typespec->llvmtype = astnode->sym.ref->typespec->llvmtype;

            result = lvalue
                ? astnode->sym.ref->llvmvalue
                : LLVMBuildLoad2(c->llvmbuilder, astnode->sym.ref->typespec->llvmtype, astnode->sym.ref->llvmvalue, "");
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
            result = astnode->llvmvalue;
        } break;

        case ASTNODE_UNOP: {
            cg_get_llvm_type(astnode->typespec);
            switch (astnode->unop.kind) {
                case UNOP_NEG: {
                    LLVMValueRef child_llvmvalue = cg_astnode(c, astnode->unop.child, false, astnode->typespec);
                    astnode->llvmvalue = LLVMBuildNeg(c->llvmbuilder, child_llvmvalue, "");
                    result = astnode->llvmvalue;
                } break;

                case UNOP_BOOLNOT: {} break;

                case UNOP_ADDR: {
                    astnode->llvmvalue = cg_astnode(c, astnode->unop.child, true, NULL);
                    result = astnode->llvmvalue;
                } break;
            }
        } break;

        case ASTNODE_DEREF: {
            cg_get_llvm_type(astnode->typespec);
            LLVMValueRef child_llvmvalue = cg_astnode(c, astnode->deref.child, false, NULL);
            // TODO: should `astnode->llvmvalue` differentiate between lvalue-llvmvalue and
            // rvalue-llvmvalue? (i know that lvalue checking is a must if we directly set
            // the result, but should we also alter astnode->llvmvalue in lieu of `lvalue`?)
            if (lvalue)
                astnode->llvmvalue = child_llvmvalue;
            else
                astnode->llvmvalue = LLVMBuildLoad2(
                    c->llvmbuilder,
                    astnode->typespec->llvmtype,
                    child_llvmvalue,
                    "");
            result = astnode->llvmvalue;
        } break;

        case ASTNODE_ACCESS: {
            astnode->typespec->llvmtype = astnode->acc.accessed->typespec->llvmtype;
            astnode->llvmvalue = astnode->acc.accessed->llvmvalue;
            result = astnode->llvmvalue;
        } break;

        case ASTNODE_EXPRSTMT: {
            result = cg_astnode(c, astnode->exprstmt, false, NULL);
        } break;

        case ASTNODE_VARIABLE_DECL: {
            if (astnode->vard.initializer && !typespec_is_comptime(astnode->typespec)) {
                LLVMValueRef initializer_llvmvalue = cg_astnode(c, astnode->vard.initializer, false, astnode->typespec);
                LLVMBuildStore(c->llvmbuilder, initializer_llvmvalue, astnode->llvmvalue);
                result = astnode->llvmvalue;
            }
        } break;

        case ASTNODE_SCOPED_BLOCK: {
            bufloop(astnode->blk.stmts, i) {
                cg_astnode(c, astnode->blk.stmts[i], false, NULL);
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
            result = astnode->llvmvalue;
        } break;

        case ASTNODE_FUNCTION_DEF: {
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
            if (typespec_is_void(astnode->typespec->func.ret_typespec)
                && (!last_stmt || (last_stmt->kind != ASTNODE_EXPRSTMT || last_stmt->exprstmt->kind != ASTNODE_RETURN))) {
                LLVMBuildRetVoid(c->llvmbuilder);
            }
            if (astnode->typespec->func.ret_typespec->kind == TS_NORETURN) {
                LLVMBuildUnreachable(c->llvmbuilder);
            }
            result = astnode->llvmvalue;
        } break;

        case ASTNODE_STRUCT: {} break;
        case ASTNODE_IMPORT: {} break;

        default: assert(0); break;
    }

    // Integer implicit casting
    if (target && typespec_is_sized_integer(target) && typespec_is_sized_integer(astnode->typespec)) {
        if (typespec_get_bytes(target) > typespec_get_bytes(astnode->typespec)) {
            if (typespec_is_signed(target)) {
                result = LLVMBuildSExt(
                    c->llvmbuilder,
                    result,
                    cg_get_llvm_type(target),
                    "");
            } else {
                result = LLVMBuildZExt(
                    c->llvmbuilder,
                    result,
                    cg_get_llvm_type(target),
                    "");
            }
        }
    }

    return result;
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

    LLVMRunPassManager(c->llvmpm, c->llvmmod);

    //printf("\n======= Optimized LLVM IR Start =======\n");
    //LLVMDumpModule(c->llvmmod);
    //printf("\n======= Optimized LLVM IR End =======\n");

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
