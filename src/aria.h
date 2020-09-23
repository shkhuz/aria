#ifndef _ARIA_H
#define _ARIA_H

#include "util/util.h"
#include "ds/ds.h"
#include "arpch.h"

typedef struct {
    const char* fpath;
} Compiler;

typedef struct {
    bool error;
    Ast* ast;
} AstOutput;

AstOutput build_ast(Compiler* self, const char* fpath);

typedef struct {
    bool error;
    Token** tokens;
} TokenOutput;

typedef struct {
	File* srcfile;
	Token** tokens;

	char* start;
	char* current;
	u64 line;

	char* last_newline;
    bool error_state;
} Lexer;

TokenOutput lex(Lexer* self, File* srcfile);

typedef enum {
    ERRLOC_GLOBAL,
    ERRLOC_FUNCTION_HEADER,
    ERRLOC_BLOCK,
    ERRLOC_NONE,
} ErrorLocation;

typedef struct {
    Ast* ast;
    u64 tokens_idx;

    bool in_function;
    bool error_panic;
    u64 error_count;
    ErrorLocation error_loc;
    bool error_state;
} Parser;

AstOutput parse(Parser* self, File* srcfile, Token** tokens);

#endif /* _ARIA_H */

