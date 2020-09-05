#include <token.h>
#include <arpch.h>

Token token_new(
	char* lexeme,
	char* start,
	char* end,
	TokenType type,
	File* file,
	u64 line,
	u64 column,
	u64 char_count) {

	Token token;
	token.lexeme = lexeme;
	token.start = start;
	token.end = end;
	token.type = type;
	token.file = file;
	token.line = line;
	token.column = column;
	token.char_count = char_count;
	return token;
}

