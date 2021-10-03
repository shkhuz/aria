#ifndef CODE_GEN_H
#define CODE_GEN_H

#include "core.h"
#include "aria.h"

typedef enum {
    REGISTER_ACC,
    REGISTER_ARG,
} RegisterGroupKind;

typedef struct {
    Srcfile* srcfile;
    char* asm_code;
} CodeGenContext;

void code_gen(CodeGenContext* c);

#endif
