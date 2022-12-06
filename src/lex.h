#ifndef LEX_H
#define LEX_H

#include "core.h"

struct CompileCtx;
struct Srcfile;

typedef struct {
    struct Srcfile* srcfile;
    const char* start, *current, *last_newl;
    bool error;
    struct CompileCtx* compile_ctx;

    // Used to prevent multiple "invalid char" errors
    bool ascii_error_table[128];
    bool invalid_char_error;
} LexCtx;

LexCtx lex_new_context(struct Srcfile* srcfile, struct CompileCtx* compile_ctx);
void lex(LexCtx* l);

#endif
