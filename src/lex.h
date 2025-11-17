#ifndef LEX_H
#define LEX_H

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
    bool invalid_char_error;
} LexCtx;

LexCtx lexctx_new(
    struct Srcfile* srcfile,
    struct CompileCtx* compilectx, 
    jmp_buf* error_handler_pos
);
void lex(LexCtx* l);

#endif
