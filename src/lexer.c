#include <lexer.h>
#include <token.h>
#include <arpch.h>

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

