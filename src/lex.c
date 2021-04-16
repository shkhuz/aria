#include <aria_core.h>
#include <aria.h>

void lexer_init(Lexer* self, SrcFile* srcfile) {
	self->srcfile = srcfile;
	self->srcfile->tokens = null;
	self->start = srcfile->contents->contents;
	self->current = self->start;
}

static void push_tok_by_type(Lexer* self, TokenType ty) {
	buf_push(
			self->srcfile->tokens,
			token_alloc(ty, self->start, self->current)
	);
}

void lexer_lex(Lexer* self) {
	for (; (u64)(self->current - self->srcfile->contents->contents) < self->srcfile->contents->len;) {
		self->start = self->current;

		switch (*self->current) {
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
			case 'Y': case 'Z': case '_':
			{
				TokenType ty = TT_IDENT;
				while (isalnum(*self->current) || *self->current == '_') {
					self->current++;
				}

				for (uint i = 0; i < stack_arr_len(keywords); i++) {
					if (stri(keywords[i]) == strni(self->start, self->current)) {
						ty = TT_KEYWORD;
					}
				}
				push_tok_by_type(self, ty);

			} break;

			case '\n': 
				self->current++;
				self->line++; 
				break;

			case '\r':
			case '\t': 
				self->current++;
				break;

			#define push_tok_by_type_for_char(self, ty) \
				self->current++; \
				push_tok_by_type(self, ty);

			case '(': push_tok_by_type_for_char(self, TT_LPAREN); break;
			case ')': push_tok_by_type_for_char(self, TT_RPAREN); break;
			case '{': push_tok_by_type_for_char(self, TT_LBRACE); break;
			case '}': push_tok_by_type_for_char(self, TT_RBRACE); break;

			default:
			{
				self->current++;
			} break;
		}
	}
}
