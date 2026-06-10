#ifndef PARSE_H
#define PARSE_H

#include "core.h"

struct CompileCtx;
struct Srcfile;
struct Token;

typedef struct {
    struct Srcfile* srcfile;
    struct Token* current, *prev;
    usize token_idx;
    struct CompileCtx* compilectx;
    bool error;
    jmp_buf* error_handler_pos;
} ParseCtx;

ParseCtx parsectx_new(
    struct Srcfile* srcfile,
    struct CompileCtx* compile_ctx,
    jmp_buf* error_handler_pos
);
void parse(ParseCtx* p);

#endif
