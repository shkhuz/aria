#ifndef CG_H
#define CG_H

#include "core.h"

#include <llvm-c/Core.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/Utils.h>

struct Typespec;
struct CompileCtx;

typedef struct {
    struct Typespec** mod_tys;
    struct Typespec* current_mod_ty;
    struct CompileCtx* compile_ctx;
    LLVMBuilderRef llvmbuilder;
    LLVMModuleRef llvmmod;
    bool error;

    LLVMTargetRef llvmtarget;
    LLVMTargetMachineRef llvmtargetmachine;
    LLVMTargetDataRef llvmtargetdatalayout;
    LLVMPassManagerRef llvmpm;
} CgCtx;

CgCtx cg_new_context(struct Typespec** mod_tys, struct CompileCtx* compile_ctx);
bool cg(CgCtx* c);

#endif
