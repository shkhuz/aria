#ifndef PARSE_H
#define PARSE_H

#include "core.h"
#include "misc.h"
#include "ast.h"

typedef struct {
    Srcfile* srcfile;
    Token* current;
    Token* prev;
    usize token_idx;
    struct CompileCtx* compile_ctx;
    jmp_buf* error_handler_pos;
} ParseCtx;

ParseCtx parse_new_context(
    Srcfile* srcfile,
    struct CompileCtx* compile_ctx,
    jmp_buf* error_handler_pos);
void parse(ParseCtx* p);

#endif
