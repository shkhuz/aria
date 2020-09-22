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
    void* parser;
    /* Parser* parser; */
} AstOutput;

AstOutput build_ast(Compiler* self, const char* fpath);

typedef struct {
	File* srcfile;
	Token** tokens;

	char* start;
	char* current;
	u64 line;

	char* last_newline;

    bool error_state;
} Lexer;

bool lex(Lexer* self, File* srcfile);

#endif /* _ARIA_H */
