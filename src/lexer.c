#include <lexer.h>
#include <token.h>
#include <error_msg.h>
#include <keywords.h>
#include <arpch.h>

Lexer lexer_new(File* srcfile) {
	Lexer lexer;
	lexer.srcfile = srcfile;
	lexer.tokens = null;
	lexer.start = srcfile->contents;
	lexer.current = lexer.start;
	lexer.line = 1;
	lexer.last_newline = lexer.start;
    lexer.error_state = ERROR_SUCCESS;
	return lexer;
}

static u64 compute_column_on_start(Lexer* self) {
	u64 column = (u64)(self->start - self->last_newline);
	if (self->line == 1) {
		column++;
	}
	return column;
}

static u64 compute_column_on_current(Lexer* self) {
	u64 column = (u64)(self->current - self->last_newline);
	if (self->line == 1) {
		column++;
	}
	return column;
}

/* static bool match(Lexer* self, char c) { */
/*     if (self->current < (self->srcfile->contents + self->srcfile->len)) { */
/*         if (*(self->current+1) == c) { */
/*             self->current++; */
/*             return true; */
/*         } */
/*     } */
/*     return false; */
/* } */

static void addt(Lexer* self, TokenType type) {
	Token* token =
		token_new_alloc(
				str_intern_range(self->start, self->current),
				self->start,
				self->current,
				type,
				self->srcfile,
				self->line,
				compute_column_on_start(self),
				(u64)(self->current - self->start)
        );

	buf_push(self->tokens, token);
}

static void error_from_start(Lexer* self, u64 char_count, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	error(
		self->srcfile,
		self->line,
		compute_column_on_start(self),
		char_count,
		fmt,
		ap
    );
	va_end(ap);
    self->error_state = ERROR_LEX;
}

static void error_from_current(Lexer* self, u64 char_count, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	error(
		self->srcfile,
		self->line,
		compute_column_on_current(self),
		char_count,
		fmt,
		ap
    );
	va_end(ap);
    self->error_state = ERROR_LEX;
}

static void identifier(Lexer* self) {
    TokenType type = T_IDENTIFIER;
	self->current++;
    bool limit_exceed_error = false;
	while (isalnum(*self->current) || *self->current == '_') {
        if ((u64)(self->current - self->start) > 256) {
            if (!limit_exceed_error) {
                error_from_current(
                        self,
                        1,
                        "identifier char-count limit[256] exceeded"
                );
            }
            limit_exceed_error = true;
        }
        self->current++;
    }
    const char* lexeme = str_intern_range(self->start, self->current);
    buf_loop(keywords, k) {
        if (str_intern((char*)lexeme) == str_intern((char*)keywords[k])) {
            type = T_KEYWORD;
            break;
        }
    }

	addt(self, type);
}

static void number(Lexer* self) {
	TokenType type = T_INTEGER;
	while (isdigit(*self->current)) self->current++;
	if (*self->current == '.') {
		self->current++;
		// TODO: add support for float64
		// TODO: check integer and float constant overflow
		type = T_FLOAT32;

		if (!isdigit(*self->current)) {
			error_from_start(
				self,
				(u64)(self->current - self->start),
				"expect fractional part after `%s`",
				str_intern_range(self->start, self->current)
            );
			return;
		}

		while (isdigit(*self->current)) self->current++;
	}

	addt(self, type);
}

static void string(Lexer* self) {
	self->current++;
	while (*self->current != '"') {
		self->current++;
		if (*self->current == '\0') {
			error_from_start(self, 1, "mismatched `\"`: encountered EOF");
			return;
		}
	}

	self->current++;
	addt(self, T_STRING);
}

static void chr(Lexer* self) {
	self->current++;
	// TODO: handle escape sequences;

	self->current += 2;
	addt(self, T_CHAR);
}

static void directive(Lexer* self) {
    self->current++;
	while (isalnum(*self->current) || *self->current == '_') {
        self->current++;
    }

    const char* lexeme = str_intern_range(self->start, self->current);
    if (str_intern("#import") == str_intern((char*)lexeme)) addt(self, T_IMPORT);
    else {
        error_from_start(
                self,
                (u64)(self->current - self->start),
                "invalid directive: `%s`",
                lexeme
        );
        return;
    }
}

Error lexer_run(Lexer* self) {
	for (;self->current != (self->srcfile->contents + self->srcfile->len);) {
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
        case 'Y': case 'Z':
            identifier(self);
            break;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            number(self);
            break;
        case '"':
            string(self);
            break;
        case '\'':
            chr(self);
            break;

        case '#':
            directive(self);
            break;

        #define char_token(type) \
            self->current++, \
            addt(self, type)

        case '+': char_token(T_PLUS); break;
        case '-': char_token(T_MINUS); break;
        case '*': char_token(T_STAR); break;
        case '/': char_token(T_FW_SLASH); break;
        case ';': char_token(T_SEMICOLON); break;
        case ':': char_token(T_COLON); break;
        case '.': char_token(T_DOT); break;
        case ',': char_token(T_COMMA); break;
        case '{': char_token(T_L_BRACE); break;
        case '}': char_token(T_R_BRACE); break;
        case '(': char_token(T_L_PAREN); break;
        case ')': char_token(T_R_PAREN); break;
        case '=': char_token(T_EQUAL); break;

        /* case ':': if (match(self, ':')) char_token(T_DOUBLE_COLON); */
        /*           else char_token(T_COLON); break; */

        #undef char_token

        case '\n':
            self->last_newline = self->current++;
            self->line++;
            break;
        case ' ':
        case '\r':
        case '\t':
            self->current++;
            break;
        default:
            error_from_current(
                    self,
                    1,
                    "invalid character"
            );
            self->current++;
            break;
		}
	}
    addt(self, T_EOF);
    return self->error_state;
}

