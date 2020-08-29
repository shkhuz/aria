#include <arpch.h>

void fatal_error(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	fprintf(stderr, "aria: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");

	va_end(ap);
	exit(1);
}

typedef enum {
	T_IDENTIFIER,
	T_KEYWORD,
	T_STRING,
	T_CHAR,
	T_INTEGER,
	T_FLOAT32,
	T_FLOAT64,
} TokenType;

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
	u64 char_count
) {
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

typedef struct {
	File* srcfile;
	Token** tokens;

	char* start;
	char* current;
	u64 line;
} Lexer;

Lexer lexer_new(void) {
	Lexer lexer;
	lexer.srcfile = null;
	lexer.tokens = null;
	lexer.start = null;
	lexer.current = null;
	lexer.line = 1;
	return lexer;
}

void lexer_lex(Lexer* l, File* srcfile) {
	l->srcfile = srcfile;
	l->start = srcfile->contents;
	l->current = l->start;

	char* contents = srcfile->contents;
	for (;l->current != (l->srcfile->contents + l->srcfile->len);) {
		l->start = l->current;

		l->current++;
	}
}

int main(int argc, char** argv) {
	if (argc < 2) {
		fatal_error("no input files specified; aborting");
	}

	const char* srcfile_name = argv[1];
	File* srcfile = file_read(srcfile_name);
	if (!srcfile) {
		fatal_error("cannot read `%s`", srcfile_name);
	}

	Lexer lexer = lexer_new();
	lexer_lex(&lexer, srcfile);
}
