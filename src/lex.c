typedef struct {
    SrcFile* srcfile;
    char* start, *current;
    u64 line;
    char* last_newline;
    bool error;
} Lexer;

u64 lexer_compute_column_from(Lexer* self, char* c) {
    u64 column = (u64)(c - self->last_newline);
    if (self->line == 1) {
        column++;
    }
    return column;
}

u64 lexer_compute_column_from_start(Lexer* self) {
    return lexer_compute_column_from(self, self->start);
}

u64 lexer_compute_column_from_current(Lexer* self) {
    return lexer_compute_column_from(self, self->current);
}

void lexer_push_tok_by_type(
        Lexer* self, 
        TokenKind kind) {
    buf_push(
            self->srcfile->tokens,
            token_alloc(
                kind, 
                strni(self->start, self->current),
                self->start, 
                self->current,
                self->srcfile,
                self->line,
                lexer_compute_column_from_start(self),
                (u64)(self->current - self->start)
            )
    );
}

void lexer_error_from_start_to_current(
        Lexer* self, 
        char* fmt, 
        ...) {
    self->error = true;
    va_list ap;
    va_start(ap, fmt);
    vmsg_user(
            MSG_KIND_ERR,
            self->srcfile,
            self->line,
            lexer_compute_column_from_start(self),
            (u64)(self->current - self->start),
            fmt, 
            ap
    );
    va_end(ap);
}

void lexer_error_from_current_to_current(
        Lexer* self, 
        char* fmt, 
        ...) {
    self->error = true;
    va_list ap;
    va_start(ap, fmt);
    vmsg_user(
            MSG_KIND_ERR,
            self->srcfile,
            self->line,
            lexer_compute_column_from_current(self),
            1,
            fmt, 
            ap
    );
    va_end(ap);
}

bool lexer_match(Lexer* self, char c) {
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

int lexer_char_to_digit(char c) {
    return c - 48;
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
                TokenKind kind = TOKEN_KIND_IDENTIFIER;
                while (isalnum(*self->current) || *self->current == '_') {
                    self->current++;
                }

                for (uint i = 0; i < stack_arr_len(keywords); i++) {
                    if (
                            stri(keywords[i]) == 
                            strni(self->start, self->current)) {
                        kind = TOKEN_KIND_KEYWORD;
                    }
                }

                lexer_push_tok_by_type(self, kind);
            } break;

            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
            {
                int base = 10;
                u64 val = 0;
                bool overflow = false;
                while (isdigit(*self->current) || *self->current == '_') {
                    if (*self->current == '_' && 
                        !isdigit(*(self->current+1))) {
                        break;
                    }

                    if (*self->current != '_') {
                        int digit = lexer_char_to_digit(*self->current);
                        if (val > (ULLONG_MAX - digit) / base) {
                            overflow = true;
                        }
                        val = val * base + digit;
                    }
                    self->current++;
                }

                if (overflow) {
                    lexer_error_from_start_to_current(
                            self,
                            "integer overflow");
                } else {
                    int bits = round_to_next_multiple(
                            get_bits_for_value(val), 
                            8);
                    TokenKind kind;
                    switch (bits) {
                        case 8: kind = TOKEN_KIND_NUMBER_8; break;
                        case 16: kind = TOKEN_KIND_NUMBER_16; break;
                        case 24:
                        case 32: kind = TOKEN_KIND_NUMBER_32; break;
                        case 40:
                        case 48:
                        case 56:
                        case 64: kind = TOKEN_KIND_NUMBER_64; break;
                        default: assert(0); break;
                    }
                    lexer_push_tok_by_type(self, kind);
                }
            } break;

            case '@':
            {
                self->current++;
                if (isalpha(*self->current) || *self->current == '_') {
                    while (isalnum(*self->current) || *self->current == '_') {
                        self->current++;
                    }

                    bool is_valid = false;
                    for (uint i = 0; i < stack_arr_len(directives); i++) {
                        if (
                                stri(directives[i]) == 
                                strni(self->start, self->current)) {
                            is_valid = true;
                        }
                    }

                    if (!is_valid) {
                        lexer_error_from_start_to_current(
                                self, 
                                "undefined directive");
                    }

                    lexer_push_tok_by_type(self, TOKEN_KIND_DIRECTIVE);
                } else {
                    lexer_error_from_current_to_current(
                            self, 
                            "invalid character after `@`");
                    self->current++;
                }
            } break;

            case '"':
            {
                self->current++;
                while (*self->current != '"') {
                    self->current++;
                    if (*self->current == '\n' || *self->current == '\0') {
                        lexer_error_from_start_to_current(
                                self, 
                                "unterminated string");
                    }
                }

                self->current++;
                lexer_push_tok_by_type(self, TOKEN_KIND_STRING);
            } break;

            case '\n': 
            {
                self->last_newline = self->current;
                self->line++; 
                self->current++;
            } break;

            case '\t':
            {
                lexer_error_from_current_to_current(
                        self,
                        "aria does not support tab characters");
                terminate_compilation();
            } break;

            case ' ':
            case '\r':
            {
                self->current++;
            } break;

#define push_tok_by_type_for_char(self, kind) \
    self->current++; \
    lexer_push_tok_by_type(self, kind);

#define push_tok_by_char_if_match(self, c, if_matched_ty, else_ty) \
{ \
    if (lexer_match(self, c)) { push_tok_by_type_for_char(self, if_matched_ty); } \
    else { push_tok_by_type_for_char(self, else_ty); } \
}

            case '(': push_tok_by_type_for_char(self, TOKEN_KIND_LPAREN); break;
            case ')': push_tok_by_type_for_char(self, TOKEN_KIND_RPAREN); break;
            case '{': push_tok_by_type_for_char(self, TOKEN_KIND_LBRACE); break;
            case '}': push_tok_by_type_for_char(self, TOKEN_KIND_RBRACE); break;
            case ';': push_tok_by_type_for_char(self, TOKEN_KIND_SEMICOLON); break;
            case ':': push_tok_by_char_if_match(
                        self, 
                        ':', 
                        TOKEN_KIND_DOUBLE_COLON, 
                        TOKEN_KIND_COLON); break;
            case ',': push_tok_by_type_for_char(self, TOKEN_KIND_COMMA); break;
            case '+': push_tok_by_type_for_char(self, TOKEN_KIND_PLUS); break;
            case '-': push_tok_by_type_for_char(self, TOKEN_KIND_MINUS); break;
            case '*': push_tok_by_type_for_char(self, TOKEN_KIND_STAR); break;
            case '=': push_tok_by_type_for_char(self, TOKEN_KIND_EQUAL); break;
            case '&': push_tok_by_type_for_char(self, TOKEN_KIND_AMPERSAND); break;
            case '/': 
            {
                if (*(self->current + 1) == '/') {
                    while (*self->current != '\n' && *self->current != '\0') {
                        self->current++;
                    }
                } else {
                    push_tok_by_type_for_char(self, TOKEN_KIND_FSLASH);
                }
            } break;

            case '\0': {
                push_tok_by_type_for_char(self, TOKEN_KIND_EOF);
                self->srcfile->tokens[buf_len(self->srcfile->tokens)-1]->lexeme 
                    = stri("<EOF>");
            } return;

            default:
            {
                self->current++;
                lexer_error_from_start_to_current(self, "invalid character");
            } break;
        }
    }

#undef push_tok_by_type_for_char
#undef push_tok_by_char_if_match
}
