#ifndef _TOKEN_H
#define _TOKEN_H

#include <token_type.h>
#include <arpch.h>

typedef struct {
	char* lexeme;
	char* start;
	char* end;
	TokenType type;
	File* srcfile;
	u64 line;
	u64 column;
	u64 char_count;
} Token;

Token token_new(
	char* lexeme,
	char* start,
	char* end,
	TokenType type,
	File* srcfile,
	u64 line,
	u64 column,
	u64 char_count);

Token* token_new_alloc(
	char* lexeme,
	char* start,
	char* end,
	TokenType type,
	File* srcfile,
	u64 line,
	u64 column,
	u64 char_count);

Token* token_from_string_type(const char* str, TokenType type);

bool is_tok_eq(Token* a, Token* b);

extern const char* tokentype_str[];

#endif /* _TOKEN_H */
