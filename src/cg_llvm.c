#include "cg.h"
#include "ast.h"
#include "type.h"
#include "buf.h"
#include "compile.h"

CgCtx cg_new_context(struct Typespec** mod_tys, struct CompileCtx* compile_ctx) {
    CgCtx c;
    c.mod_tys = mod_tys;
    c.compile_ctx = compile_ctx;
    return c;
}

static LLVMTypeRef cg_get_llvm_type(Typespec* typespec) {
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
                    return LLVMIntType(typespec_get_bytes(typespec) * 8);

                case PRIM_bool:
                    return LLVMInt8Type();
                case PRIM_void:
                    return LLVMVoidType();

                case PRIM_integer:
                    return LLVMInt64Type();

                default: assert(0 && "cg_get_llvm_type()");
            }
        } break;

        default: assert(0 && "cg_get_llvm_type()");
    }
    assert(0);
    return NULL;
}

static void cg_top_level_decls_prec1(CgCtx* c, AstNode* astnode) {
    switch (astnode->kind) {
        case ASTNODE_STRUCT: {
            astnode->llvmtype = LLVMStructCreateNamed(
                LLVMGetGlobalContext(),
                token_tostring(astnode->strct.identifier));
        } break;
    }
}

static void cg_top_level_decls_prec2(CgCtx* c, AstNode* astnode) {
    switch (astnode->kind) {
        case ASTNODE_VARIABLE_DECL: {
            astnode->llvmtype = cg_get_llvm_type(astnode->typespec);
            astnode->llvmvalue = LLVMAddGlobal(
                c->llvmmod,
                astnode->llvmtype,
                token_tostring(astnode->vard.identifier));
        } break;

        case ASTNODE_FUNCTION_DEF: {
            AstNodeFunctionHeader* header = &astnode->funcdef.header->funch;
            LLVMTypeRef* param_llvmtypes = NULL;
            bufloop(header->params, i) {
                header->params[i]->llvmtype = cg_get_llvm_type(header->params[i]->typespec);
                bufpush(param_llvmtypes, header->params[i]->llvmtype);
            }
            LLVMTypeRef ret_llvmtype = cg_get_llvm_type(astnode->typespec->func.ret_typespec);
            astnode->funcdef.header->funch.ret_typespec->llvmtype = ret_llvmtype;

            astnode->llvmtype = LLVMFunctionType(
                ret_llvmtype,
                param_llvmtypes,
                buflen(header->params),
                false);
            astnode->llvmvalue = LLVMAddFunction(
                c->llvmmod,
                header->name,
                astnode->llvmtype);
        } break;

        case ASTNODE_STRUCT: {
            LLVMTypeRef* field_llvmtypes = NULL;
            bufloop(astnode->strct.fields, i) {
                astnode->strct.fields[i]->llvmtype = cg_get_llvm_type(astnode->strct.fields[i]->typespec);
                bufpush(field_llvmtypes, astnode->strct.fields[i]->llvmtype);
            }

            LLVMStructSetBody(
                astnode->llvmtype,
                field_llvmtypes,
                buflen(field_llvmtypes),
                false);
        } break;
    }
}

LLVMValueRef cg_astnode(CgCtx* c, AstNode* astnode, bool lvalue, Typespec* target, LLVMTypeRef target_llvmtype) {
    if (typespec_is_unsized_integer(astnode->typespec) && target) {
        return LLVMConstInt(
            typespec_is_sized_integer(target) ? target_llvmtype : LLVMInt64Type(),
            astnode->typespec->prim.integer.neg ? (-astnode->typespec->prim.integer.d[0]) : astnode->typespec->prim.integer.d[0],
            astnode->typespec->prim.integer.neg);
    }

    switch (astnode->kind) {
        // Integer literal case handled above
        case ASTNODE_INTEGER_LITERAL: assert(0); break;

        case ASTNODE_FUNCTION_CALL: {
            LLVMValueRef* arg_llvmvalues = NULL;
            if (buflen(astnode->funcc.args) != 0) {
                bufloop(astnode->funcc.args, i) {
                    bufpush(arg_llvmvalues, cg_astnode(
                        c,
                        astnode->funcc.args[i],
                        false,
                        astnode->funcc.callee->sym.ref->funcdef.header->funch.params[i]->typespec,
                        astnode->funcc.callee->sym.ref->funcdef.header->funch.params[i]->llvmtype));
                }
            }

            astnode->llvmtype = astnode->funcc.callee->sym.ref->funcdef.header->funch.ret_typespec->llvmtype;
            astnode->llvmvalue = LLVMBuildCall2(
                c->llvmbuilder,
                astnode->funcc.callee->sym.ref->llvmtype,
                astnode->funcc.callee->sym.ref->llvmvalue,
                arg_llvmvalues,
                buflen(astnode->funcc.args),
                "");
            return astnode->llvmvalue;
        } break;

        case ASTNODE_SYMBOL: {
            astnode->llvmtype = astnode->sym.ref->llvmtype;

            return lvalue
                ? astnode->sym.ref->llvmvalue
                : LLVMBuildLoad2(c->llvmbuilder, astnode->sym.ref->llvmtype, astnode->sym.ref->llvmvalue, "");
        } break;

        case ASTNODE_EXPRSTMT: {
            cg_astnode(c, astnode->exprstmt, false, NULL, NULL);
        } break;

        case ASTNODE_VARIABLE_DECL: {
            if (astnode->vard.initializer) {
                LLVMValueRef initializer_llvmvalue = cg_astnode(c, astnode->vard.initializer, false, astnode->typespec, astnode->llvmtype);
                LLVMSetInitializer(astnode->llvmvalue, initializer_llvmvalue);
                return astnode->llvmvalue;
            }
            return NULL;
        } break;

        case ASTNODE_SCOPED_BLOCK: {
            bufloop(astnode->blk.stmts, i) {
                cg_astnode(c, astnode->blk.stmts[i], false, NULL, NULL);
            }
            return NULL;
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
                    params[i]->llvmvalue = LLVMBuildAlloca(c->llvmbuilder, params[i]->llvmtype, strcat(params[i]->paramd.name, ".addr"));
                    LLVMBuildStore(c->llvmbuilder, param_llvmvalues[i], params[i]->llvmvalue);
                }
            }

            AstNode** locals = astnode->funcdef.locals;
            bufloop(locals, i) {
                locals[i]->llvmtype = cg_get_llvm_type(locals[i]->typespec);
                locals[i]->llvmvalue = LLVMBuildAlloca(
                    c->llvmbuilder,
                    locals[i]->llvmtype,
                    locals[i]->vard.name);
            }

            LLVMValueRef body_llvmvalue = cg_astnode(c, astnode->funcdef.body, false, NULL, NULL);
            return astnode->llvmvalue;
        } break;
    }
}

void cg(CgCtx* c) {
    c->llvmbuilder = LLVMCreateBuilder();
    c->llvmmod = LLVMModuleCreateWithName("root");

    bufloop(c->mod_tys, i) {
        Srcfile* srcfile = c->mod_tys[i]->mod.srcfile;
        bufloop(srcfile->astnodes, j) {
            cg_top_level_decls_prec1(c, srcfile->astnodes[j]);
        }
    }

    bufloop(c->mod_tys, i) {
        Srcfile* srcfile = c->mod_tys[i]->mod.srcfile;
        bufloop(srcfile->astnodes, j) {
            cg_top_level_decls_prec2(c, srcfile->astnodes[j]);
        }
    }

    bufloop(c->mod_tys, i) {
        Srcfile* srcfile = c->mod_tys[i]->mod.srcfile;
        bufloop(srcfile->astnodes, j) {
            cg_astnode(c, srcfile->astnodes[j], false, NULL, NULL);
        }
    }

    printf("\n======= LLVM IR =======\n");
    LLVMDumpModule(c->llvmmod);
}
