#ifndef CG_H
#define CG_H

#include "core.h"

#include <llvm-c/Core.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/Utils.h>

struct Typespec;
struct CompileCtx;
struct AstNode;

typedef struct {
    struct Typespec** mod_tys;
    struct CompileCtx* compile_ctx;
    bool error;

    struct Typespec* current_mod_ty;
    struct AstNode* current_func;
    LLVMBasicBlockRef current_bb;
    LLVMBasicBlockRef* while_cond_stack;
    LLVMBasicBlockRef* while_end_stack;

    LLVMBuilderRef llvmbuilder;
    LLVMModuleRef llvmmod;
    LLVMTargetRef llvmtarget;
    LLVMTargetMachineRef llvmtargetmachine;
    LLVMTargetDataRef llvmtargetdatalayout;
    LLVMPassManagerRef llvmpm;

    LLVMTypeRef llvmptrtype;
} CgCtx;

CgCtx cg_new_context(struct Typespec** mod_tys, struct CompileCtx* compile_ctx);
bool cg(CgCtx* c);

#endif
