#ifndef LEX_H
#define LEX_H

#include "core.h"

struct CompileCtx;
struct Srcfile;

typedef struct {
    struct Srcfile* srcfile;
    const char* start, *current, *lastnl;
    bool error;
    struct CompileCtx* compilectx;
    jmp_buf* error_handler_pos;

    // Used to prevent multiple "invalid char" errors
    bool ascii_error_table[128];

    // Used at the of lexing to add a note
    // reminding that each invalid character error
    // is only shown once.
    bool invalid_char_error;
} LexCtx;

LexCtx lexctx_new(
    struct Srcfile* srcfile,
    struct CompileCtx* compilectx, 
    jmp_buf* error_handler_pos
);
void lex(LexCtx* l);

#endif
