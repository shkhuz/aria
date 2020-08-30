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
	T_IDENTIFIER = 0,
	T_KEYWORD,
	T_STRING,
	T_CHAR,
	T_INTEGER,
	T_FLOAT32,
	T_FLOAT64,
} TokenType;

const char* tokentype_str[] = {
	"T_IDENTIFIER",
	"T_KEYWORD",
	"T_STRING",
	"T_CHAR",
	"T_INTEGER",
	"T_FLOAT32",
	"T_FLOAT64",
};

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

void lexer_addt(Lexer* l, TokenType type) {
	Token token =
		token_new(
				str_intern_range(l->start, l->current),
				l->start,
				l->current,
				type,
				l->srcfile,
				l->line,
				0, /* TODO: compute column */
				(u64)(l->current - l->start));
	Token* token_heap = malloc(sizeof(Token));
	memcpy(token_heap, &token, sizeof(Token));

	buf_push(l->tokens, token_heap);
}

void lexer_identifier(Lexer* l) {
	l->current++;
	while (isalnum(*l->current) || *l->current == '_') {
		l->current++;
	}

	lexer_addt(l, T_IDENTIFIER);
}

void lexer_string(Lexer* l) {
	l->start++;
	l->current++;
	while (*l->current != '"') {
		l->current++;
		// TODO: handle EOF here;
	}

	lexer_addt(l, T_STRING);
	l->current++;
}

void lexer_char(Lexer* l) {
	l->start++;
	l->current++;
	// TODO: handle escape sequences;

	l->current++;
	lexer_addt(l, T_CHAR);
	l->current++;
}

void lexer_lex(Lexer* l, File* srcfile) {
	l->srcfile = srcfile;
	l->start = srcfile->contents;
	l->current = l->start;

	char* contents = srcfile->contents;
	for (;l->current != (l->srcfile->contents + l->srcfile->len);) {
		l->start = l->current;

		switch (*l->current) {
			case 'a': case 'b': case 'c': case 'd': case 'e':
			case 'f': case 'g': case 'h': case 'i': case 'j':
			case 'k': case 'l': case 'm': case 'n': case 'o':
			case 'p': case 'q': case 'r': case 's': case 't':
			case 'u': case 'v': case 'w': case 'x': case 'y':
			case 'z': case 'A': case 'B': case 'C': case 'D':
			case 'E': case 'F': case 'G': case 'H': case 'I':
			case 'J': case 'K': case 'L': case 'M': case 'N':
			case 'O': case 'P': case 'Q': case 'R': case 'S':
			case 'T': case 'U': case 'V': case 'W': case 'X':
			case 'Y': case 'Z':
				lexer_identifier(l);
				break;
			case '"':
				lexer_string(l);
				break;
			case '\'':
				lexer_char(l);
				break;
			default:
				l->current++;
				break;
		}
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

	buf_loop(lexer.tokens, t) {
		printf(
			"[%15s: %s]\n",
			tokentype_str[lexer.tokens[t]->type],
			lexer.tokens[t]->lexeme);
	}
}

