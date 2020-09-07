#ifndef _LEXER_H
#define _LEXER_H

#include <token_type.h>
#include <token.h>
#include <error_value.h>
#include <arpch.h>

typedef struct {
	File* srcfile;
	Token** tokens;

	char* start;
	char* current;
	u64 line;

	char* last_newline;

    Error error_state;
} Lexer;

Lexer lexer_new(File* srcfile);
Error lexer_run(Lexer* self);

#endif /* _LEXER_H */
