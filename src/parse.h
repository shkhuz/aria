#ifndef PARSE_H
#define PARSE_H

#include "core.h"

struct CompileCtx;
struct Srcfile;
struct Token;

typedef struct {
    struct Srcfile* src;
    usize current;
    struct CompileCtx* compilectx;
    bool error;
    jmp_buf* error_handler_pos;
    // LIFO scratchpad for nextra.
    // Used to temporarily store child indices 
    // until the end of parsing of a node.
    int* sextra;
} ParseCtx;

ParseCtx parsectx_new(
    struct Srcfile* src,
    struct CompileCtx* compile_ctx,
    jmp_buf* error_handler_pos
);
void parse(ParseCtx* p);

#endif
