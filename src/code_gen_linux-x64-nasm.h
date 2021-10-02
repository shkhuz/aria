#ifndef CODE_GEN_H
#define CODE_GEN_H

#include "core.h"
#include "aria.h"

typedef enum {
    REGISTER_RAX,
    REGISTER_RCX,
} RegisterKind;

typedef struct {
    Srcfile* srcfile;
    char* asm_code;
    RegisterKind acc_reg;
} CodeGenContext;

void code_gen(CodeGenContext* c);

#endif
