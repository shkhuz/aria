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
    struct CompileCtx* compile_ctx;
    LLVMBuilderRef llvmbuilder;
    LLVMModuleRef llvmmod;
} CgCtx;

CgCtx cg_new_context(struct Typespec** mod_tys, struct CompileCtx* compile_ctx);
void cg(CgCtx* c);

#endif
