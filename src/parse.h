#ifndef PARSE_H
#define PARSE_H

#include "core.h"
#include "ast.h"

struct CompileCtx;
struct Srcfile;

typedef struct {
    struct Srcfile* srcfile;
    Token* current;
    Token* prev;
    usize token_idx;
    struct CompileCtx* compile_ctx;
    bool error;
    jmp_buf* error_handler_pos;

    AstNode*** loop_breaks;
    AstNode*** loop_continues;
} ParseCtx;

ParseCtx parse_new_context(
    struct Srcfile* srcfile,
    struct CompileCtx* compile_ctx,
    jmp_buf* error_handler_pos);
void parse(ParseCtx* p);

#endif
