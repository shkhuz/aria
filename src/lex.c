#include "arpch.h"
#include "ds/ds.h"
#include "error_msg.h"
#include "aria.h"

#define check_eof_from_offset(offset) \
    if (*(self->current+offset) == '\0') return

static u64 compute_column_on(Lexer* self, char* c) {
	u64 column = (u64)(c - self->last_newline);
	if (self->line == 1) {
		column++;
	}
	return column;
}

static u64 compute_column_on_start(Lexer* self) {
    return compute_column_on(self, self->start);
}

static u64 compute_column_on_current(Lexer* self) {
    return compute_column_on(self, self->current);
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

static void addt_from_lexeme_start_end(
        Lexer* self,
        char* lexeme,
        char* start,
        char* end,
        TokenType type) {
	Token* token =
		token_new_alloc(
                lexeme,
				start,
				end,
				type,
				self->srcfile,
				self->line,
				compute_column_on(self, start),
				(u64)(end - start)
        );
	buf_push(self->tokens, token);
}

static void addt(Lexer* self, TokenType type) {
    addt_from_lexeme_start_end(
            self,
            str_intern_range(self->start, self->current),
            self->start,
            self->current,
            type
    );
}

static void error_from_start(Lexer* self, u64 char_count, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	msg(
        MSG_ERROR,
		self->srcfile,
		self->line,
		compute_column_on_start(self),
		char_count,
		fmt,
		ap
    );
	va_end(ap);
    self->error_state = true;
}

static void error_from_current(Lexer* self, u64 char_count, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	msg(
        MSG_ERROR,
		self->srcfile,
		self->line,
		compute_column_on_current(self),
		char_count,
		fmt,
		ap
    );
	va_end(ap);
    self->error_state = true;
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

u8 char_to_digit[256] = {
    ['0'] = 0,
    ['1'] = 1,
    ['2'] = 2,
    ['3'] = 3,
    ['4'] = 4,
    ['5'] = 5,
    ['6'] = 6,
    ['7'] = 7,
    ['8'] = 8,
    ['9'] = 9,
    ['a'] = 10, ['A'] = 10,
    ['b'] = 11, ['B'] = 11,
    ['c'] = 12, ['C'] = 12,
    ['d'] = 13, ['D'] = 13,
    ['e'] = 14, ['E'] = 14,
    ['f'] = 15, ['F'] = 15,
};

typedef struct {
    u64 limit;
    TokenType type;
} IntTypeLimit;

static IntTypeLimit parse_integer_limit_suffix(char* str) {
    while (isdigit(*str)) str++;
    IntTypeLimit deflt = (IntTypeLimit){ UINT64_MAX, T_INTEGER_U64 };

    bool unsignd = true;
    switch (*str++) {
    case 'U':
    case 'u': goto parse_limit_size; break;
    case 'I':
    case 'i': unsignd = false; goto parse_limit_size; break;
    default: return deflt;
    }

parse_limit_size:
    switch (*str++) {
    case '8':
        if (unsignd) return (IntTypeLimit){ UINT8_MAX, T_INTEGER_U8 };
        else return (IntTypeLimit){ llabs(INT8_MAX), T_INTEGER_I8 };
        break;
    case '1':
        if (*str == '6') {
            if (unsignd) return (IntTypeLimit){ UINT16_MAX, T_INTEGER_U16 };
            else return (IntTypeLimit){ llabs(INT16_MAX), T_INTEGER_I16 };
        }
        break;
    case '3':
        if (*str == '2') {
            if (unsignd) return (IntTypeLimit){ UINT32_MAX, T_INTEGER_U32 };
            else return (IntTypeLimit){ llabs(INT32_MAX), T_INTEGER_I32 };
        }
        break;
    case '6':
        if (*str == '4') {
            if (unsignd) return (IntTypeLimit){ UINT64_MAX, T_INTEGER_U64 };
            else return (IntTypeLimit){ llabs(INT64_MAX), T_INTEGER_I64 };
        }
        break;
    }

    return deflt;
}

static void skip_integer_suffix(Lexer* self) {
    check_eof_from_offset(0);
    switch (*self->current) {
    case 'U':
    case 'u':
    case 'I':
    case 'i': goto limit_size; break;
    default: return;
    }

limit_size:
    check_eof_from_offset(1);
    switch (*(self->current+1)) {
    case '8': self->current += 2; break;

    case '1': {
        check_eof_from_offset(2);
        if (*(self->current+2) == '6') self->current += 3;
    } break;

    case '3': {
        check_eof_from_offset(2);
        if (*(self->current+2) == '2') self->current += 3;
    } break;

    case '6': {
        check_eof_from_offset(2);
        if (*(self->current+2) == '4') self->current += 3;
    } break;
    }
}

static void number(Lexer* self) {
    u64 to_int = 0;
    bool integer_overflow = false;
    IntTypeLimit int_type = parse_integer_limit_suffix(self->current);
	while (isdigit(*self->current)) {
        u8 digit = char_to_digit[*self->current];
        if (to_int > (int_type.limit-digit)/10) {
            integer_overflow = true;
        }
        to_int = to_int*10 + digit;
        self->current++;
    }

	if (*self->current == '.') {
		self->current++;
		// TODO: add support for float64
		// TODO: check float constant overflow
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
        addt(self, T_FLOAT32);
	}
    else {
        char* lexeme = str_intern_range(self->start, self->current);
        /* suffix is already parsed by `parse_integer_limit_suffix`,
         * skip it */
        skip_integer_suffix(self);
        addt_from_lexeme_start_end(
                self,
                lexeme,
                self->start,
                self->current,
                int_type.type
        );
        if (integer_overflow) {
            error_from_start(
                    self,
                    (u64)(self->current - self->start),
                    "integer overflow"
            );
        }
    }
}

static void string(Lexer* self) {
    self->current++;
    while (*self->current != '"') {
        self->current++;
        if (*self->current == '\n' || *self->current == '\0') {
            error_from_start(
                    self,
                    (u64)(self->current - self->start),
                    "missing terminating `\"`"
            );
            return;
        }
    }
    self->current++;
    addt(self, T_STRING);
}

TokenOutput lex(Lexer* self, File* srcfile) {
    self->srcfile = srcfile;
    self->tokens =  null;
	self->start = srcfile->contents;
	self->current = self->start;
	self->line = 1;
	self->last_newline = self->start;
    self->error_state = false;

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

        #define char_token(type) \
            self->current++, \
            addt(self, type)

        case ';': char_token(T_SEMICOLON); break;
        case ':': char_token(T_COLON); break;
        case ',': char_token(T_COMMA); break;
        case '+': char_token(T_PLUS); break;
        case '-': char_token(T_MINUS); break;
        case '*': char_token(T_STAR); break;
        case '&': char_token(T_AMPERSAND); break;
        case '{': char_token(T_L_BRACE); break;
        case '}': char_token(T_R_BRACE); break;
        case '(': char_token(T_L_PAREN); break;
        case ')': char_token(T_R_PAREN); break;
        case '=': char_token(T_EQUAL); break;

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
    return (TokenOutput){ self->error_state, self->tokens };
}

