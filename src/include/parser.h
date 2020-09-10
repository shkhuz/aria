#ifndef _PARSER_H
#define _PARSER_H

#include <token.h>
#include <stmt.h>
#include <error_value.h>
#include <arpch.h>

typedef enum {
    ERRLOC_GLOBAL,
    ERRLOC_FUNCTION_HEADER,
    ERRLOC_BLOCK,
    ERRLOC_NONE,
} ErrorLocation;

typedef struct {
    File* srcfile;
    Token** tokens;
    u64 tokens_idx;
    Stmt** stmts;
    Stmt** decls;

    bool error_panic;
    u64 error_count;
    ErrorLocation error_loc;
    Error error_state;
} Parser;

Parser parser_new(File* srcfile, Token** tokens);
Error parser_run(Parser* self);

#endif /* _PARSER_H */
