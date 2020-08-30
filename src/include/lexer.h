#ifndef _LEXER_H
#define _LEXER_H

#include <token_type.h>
#include <token.h>
#include <arpch.h>

typedef struct {
	File* srcfile;
	Token** tokens;

	char* start;
	char* current;
	u64 line;

	char* last_newline;
} Lexer;

Lexer lexer_new(void);
void lexer_lex(Lexer* l, File* srcfile);

#endif /* _LEXER_H */
