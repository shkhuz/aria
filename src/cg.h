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
    struct {
        LLVMBasicBlockRef brcondbb;
        LLVMBasicBlockRef brbodybb;
        LLVMBasicBlockRef nextbb;
        LLVMBasicBlockRef endifexprbb;
    } ifstate;

    LLVMBuilderRef llvmbuilder;
    LLVMModuleRef llvmmod;
    LLVMTargetRef llvmtarget;
    LLVMTargetMachineRef llvmtargetmachine;
    LLVMTargetDataRef llvmtargetdatalayout;
    LLVMPassManagerRef llvmpm;
} CgCtx;

CgCtx cg_new_context(struct Typespec** mod_tys, struct CompileCtx* compile_ctx);
bool cg(CgCtx* c);

#endif
