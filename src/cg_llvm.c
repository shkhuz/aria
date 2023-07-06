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
                    return LLVMInt1Type();
                case PRIM_void:
                    return LLVMVoidType();

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

void cg_astnode(CgCtx* c, AstNode* astnode) {
    switch (astnode->kind) {
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
                    params[i]->llvmvalue = LLVMBuildAlloca(c->llvmbuilder, params[i]->llvmtype, "");
                    LLVMBuildStore(c->llvmbuilder, param_llvmvalues[i], params[i]->llvmvalue);
                }
            }
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
            cg_astnode(c, srcfile->astnodes[j]);
        }
    }

    printf("\n======= LLVM IR =======\n");
    LLVMDumpModule(c->llvmmod);
}
