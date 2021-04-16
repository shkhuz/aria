#include <aria_core.h>
#include <aria.h>

void lexer_init(Lexer* self, SrcFile* srcfile) {
	self->srcfile = srcfile;
	self->srcfile->tokens = null;
	self->start = srcfile->contents->contents;
	self->current = self->start;
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
				
				buf_push(self->srcfile->tokens,
						 token_alloc(ty, self->start, self->current)
				);
			} break;

			default:
			{
				self->current++;
			} break;
		}
	}
}
