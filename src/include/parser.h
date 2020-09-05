#ifndef _PARSER_H
#define _PARSER_H

#include <token.h>
#include <stmt.h>
#include <arpch.h>

typedef struct {
    File* srcfile;
    Token** tokens;
    u64 tokens_idx;
    Stmt** stmts;

    bool error_panic;
    u64 error_count;
} Parser;

Parser parser_new(File* srcfile, Token** tokens);
void parser_run(Parser* self);

#endif /* _PARSER_H */
