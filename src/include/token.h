#ifndef _TOKEN_H
#define _TOKEN_H

#include <token_type.h>
#include <arpch.h>

typedef struct {
	char* lexeme;
	char* start;
	char* end;
	TokenType type;
	File* file;
	u64 line;
	u64 column;
	u64 char_count;
} Token;

Token token_new(
	char* lexeme,
	char* start,
	char* end,
	TokenType type,
	File* file,
	u64 line,
	u64 column,
	u64 char_count);

extern const char* tokentype_str[];

#endif /* _TOKEN_H */
