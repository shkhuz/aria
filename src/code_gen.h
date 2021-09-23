#ifndef CODE_GEN_H
#define CODE_GEN_H

#include "core.h"
#include "aria.h"

typedef struct {
    Srcfile* srcfile;
    char* asm_code;
    size_t last_stack_offset;
} CodeGenContext;

void code_gen(CodeGenContext* c);

#endif
