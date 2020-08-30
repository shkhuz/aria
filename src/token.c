#include <token.h>
#include <arpch.h>

const char* tokentype_str[] = {
	"T_IDENTIFIER",
	"T_KEYWORD",
	"T_STRING",
	"T_CHAR",
	"T_INTEGER",
	"T_FLOAT32",
	"T_FLOAT64",
};

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

