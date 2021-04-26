#include <aria_core.h>
#include <aria.h>

static u64 compute_column_from(Lexer* self, char* c) {
	u64 column = (u64)(c - self->last_newline);
	if (self->line == 1) {
		column++;
	}
	return column;
}

static u64 compute_column_from_start(Lexer* self) {
	return compute_column_from(self, self->start);
}

static void push_tok_by_type(Lexer* self, TokenType ty) {
	buf_push(
			self->srcfile->tokens,
			token_alloc(
				ty, 
				strni(self->start, self->current),
				self->start, 
				self->current,
				self->srcfile,
				self->line,
				compute_column_from_start(self),
				(u64)(self->current - self->start)
			)
	);
}

static void error_from_start_to_current(Lexer* self, u32 code, char* fmt, ...) {
	self->error = true;
	va_list ap;
	va_start(ap, fmt);
	vmsg_user(
			MSG_TY_ERR,
			self->srcfile,
			self->line,
			compute_column_from_start(self),
			(u64)(self->current - self->start),
			code,
			fmt, 
			ap
	);
	va_end(ap);
}

static bool match(Lexer* self, char c) {
	if (*(self->current + 1) == c) {
		self->current++;
		return true;
	}
	return false;
}

void lexer_init(Lexer* self, SrcFile* srcfile) {
	self->srcfile = srcfile;
	self->srcfile->tokens = null;
	self->start = srcfile->contents->contents;
	self->current = self->start;
	self->line = 1;
	self->last_newline = self->srcfile->contents->contents;
	self->error = false;
}

void lexer_lex(Lexer* self) {
	for (;;) {
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

			case '#':
			{
				self->current++;
				if (isalpha(*self->current) || *self->current == '_') {
					while (isalnum(*self->current) || *self->current == '_') {
						self->current++;
					}

					bool is_valid = false;
					for (uint i = 0; i < stack_arr_len(directives); i++) {
						if (stri(directives[i]) == strni(self->start + 1, self->current)) {
							is_valid = true;
						}
					}

					if (!is_valid) {
						error_from_start_to_current(self, ERROR_UNDEFINED_DIRECTIVE);
					}

					push_tok_by_type(self, TT_DIRECTIVE);
				}
			} break;

			case '"':
			{
				self->current++;
				while (*self->current != '"') {
					self->current++;
					if (*self->current == '\n' || *self->current == '\0') {
						error_from_start_to_current(self, ERROR_UNTERMINATED_STRING);
					}
				}

				self->current++;
				push_tok_by_type(self, TT_STRING);
			} break;

			case '\n': 
			{
				self->last_newline = self->current;
				self->line++; 
				self->current++;
			} break;

			case ' ':
			case '\r':
			case '\t': 
				self->current++;
				break;

			#define push_tok_by_type_for_char(self, ty) \
				self->current++; \
				push_tok_by_type(self, ty);

			#define push_tok_by_char_if_match(self, c, if_matched_ty, else_ty) \
			{ \
				if (match(self, c)) { push_tok_by_type_for_char(self, if_matched_ty); } \
				else { push_tok_by_type_for_char(self, else_ty); } \
			}

			case '(': push_tok_by_type_for_char(self, TT_LPAREN); break;
			case ')': push_tok_by_type_for_char(self, TT_RPAREN); break;
			case '{': push_tok_by_type_for_char(self, TT_LBRACE); break;
			case '}': push_tok_by_type_for_char(self, TT_RBRACE); break;
			case ';': push_tok_by_type_for_char(self, TT_SEMICOLON); break;
			case ':': push_tok_by_char_if_match(self, ':', TT_DOUBLE_COLON, TT_COLON); break;
			case ',': push_tok_by_type_for_char(self, TT_COMMA); break;
			case '+': push_tok_by_type_for_char(self, TT_PLUS); break;
			case '-': push_tok_by_type_for_char(self, TT_MINUS); break;
			case '*': push_tok_by_type_for_char(self, TT_STAR); break;
			case '=': push_tok_by_type_for_char(self, TT_EQUAL); break;
			case '&': push_tok_by_type_for_char(self, TT_AMPERSAND); break;
			case '/': 
			{
				if (*(self->current + 1) == '/') {
					while (*self->current != '\n' && *self->current != '\0') {
						self->current++;
					}
				} else {
					push_tok_by_type_for_char(self, TT_FSLASH);
				}
			} break;

			case '\0': push_tok_by_type_for_char(self, TT_EOF); return;

			default:
			{
				self->current++;
				error_from_start_to_current(self, ERROR_INVALID_CHAR);
			} break;
		}
	}
}
