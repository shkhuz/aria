typedef struct {
    Srcfile* srcfile;
    char* start, *current, *last_newline;
    size_t line;
    size_t lines;
    bool error;
} LexContext;

static bool ascii_error_table[128];
static bigint numbase_bi;
static bigint digit_bi;
static bigint val_bi;

void init_lex() {
    bigint_init_u64(&numbase_bi, 10);
    bigint_init(&digit_bi);
    bigint_init(&val_bi);
}

size_t lex_get_col(LexContext* l, char* c) {
    size_t col = c - l->last_newline;
    if (l->line == 1) 
        col++;
    return col;
}

size_t lex_get_col_from_start(LexContext* l) {
    return lex_get_col(l, l->start);
}

size_t lex_get_col_from_current(LexContext* l) {
    return lex_get_col(l, l->current);
}

void lex_push_tok(LexContext* l, TokenKind kind) {
    ALLOC_WITH_TYPE(token, Token);
    token->kind = kind;
    token->lexeme = kind == TOKEN_KIND_EOF ? "EOF" : std::string(l->start, l->current);
    token->start = l->start;
    token->end = l->current;
    token->srcfile = l->srcfile;
    token->line = l->line;
    token->col = lex_get_col_from_start(l);
    token->ch_count = l->current - l->start;
    l->srcfile->tokens.push_back(token);
}

void lex_push_tok_adv(LexContext* l, TokenKind kind) {
    l->current++;
    lex_push_tok(l, kind);
}

void lex_push_tok_adv_cond(
        LexContext* l, 
        char c, 
        TokenKind matched,
        TokenKind not_matched) {
    if (*(l->current+1) == c) {
        l->current++;
        lex_push_tok_adv(l, matched);
    }
    else {
        lex_push_tok_adv(l, not_matched);
    }
}

template<typename... Args>
void lex_error(
        LexContext* l,
        size_t col,
        size_t ch_count,
        const std::string& fmt,
        Args... args) {
    l->error = true;
    default_msg(
            MSG_KIND_ERROR,
            l->srcfile,
            l->line,
            col,
            ch_count,
            fmt,
            args...);
    // TODO: should we terminate compilation or continue?
}

template<typename... Args>
void lex_error_from_start(LexContext* l, const std::string& fmt, Args... args) {
    lex_error(
            l,
            lex_get_col_from_start(l),
            l->current - l->start,
            fmt,
            args...);
}

template<typename... Args>
void lex_error_from_current(LexContext* l, const std::string& fmt, Args... args) {
    lex_error(
            l,
            lex_get_col_from_current(l),
            1,
            fmt,
            args...);
}

void lex(LexContext* l) {
    l->start = l->srcfile->handle->contents;
    l->current = l->start;
    l->last_newline = l->start;
    l->line = 1;
    l->error = false;

    for (;;) {
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
            case 'Y': case 'Z': case '_': {
                TokenKind kind = TOKEN_KIND_IDENTIFIER;
                while (isalnum(*l->current) || *l->current == '_')
                    l->current++;
                for (size_t i = 0; i < STCK_ARR_LEN(aria_keywords); i++) {
                    /* if (stri(aria_keywords[i]) == strni(l->start, l->current)) { */
                    if (strncmp(aria_keywords[i].data(), l->start, aria_keywords[i].size()) == 0) {
                        kind = TOKEN_KIND_KEYWORD;
                        break;
                    }
                }
                lex_push_tok(l, kind);
            } break;

            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9': {
                while (isdigit(*l->current) || *l->current == '_') {
                    if (*l->current == '_' && !isdigit(*(l->current+1))) {
                        break;
                    }

                    if (*l->current != '_') {
                        int digit = char_to_digit(*l->current);
                        bigint_set_u64(&digit_bi, (u64)digit);
                        bigint_mul(&val_bi, &numbase_bi, &val_bi);
                        bigint_add(&val_bi, &digit_bi, &val_bi);
                        // no need to clear digit_bi because
                        // bigint_set_u64 will clear the upper
                        // half after adding the required digits.
                    }
                    l->current++;
                }

                lex_push_tok(l, TOKEN_KIND_INTEGER);
                bigint val_bi_copy;
                bigint_init(&val_bi_copy);
                bigint_copy(&val_bi, &val_bi_copy);
                l->srcfile->tokens[l->srcfile->tokens.size()-1]
                    ->integer.val = val_bi_copy;
                bigint_clear(&val_bi);
            } break;
            
            case ' ':
            case '\t':
            case '\r': {
                l->current++;
            } break;

            case '\n': {
                l->last_newline = l->current;
                l->current++;
                l->line++;
            } break;

            case '\0': {
                l->lines = l->line-1;
                lex_push_tok_adv(l, TOKEN_KIND_EOF);
            } return;

            case '{': lex_push_tok_adv(l, TOKEN_KIND_LBRACE); break;
            case '}': lex_push_tok_adv(l, TOKEN_KIND_RBRACE); break;
            case '[': lex_push_tok_adv(l, TOKEN_KIND_LBRACK); break;
            case ']': lex_push_tok_adv(l, TOKEN_KIND_RBRACK); break;
            case '(': lex_push_tok_adv(l, TOKEN_KIND_LPAREN); break;
            case ')': lex_push_tok_adv(l, TOKEN_KIND_RPAREN); break;
            case ':': lex_push_tok_adv(l, TOKEN_KIND_COLON); break;
            case ';': lex_push_tok_adv(l, TOKEN_KIND_SEMICOLON); break;
            case '.': lex_push_tok_adv(l, TOKEN_KIND_DOT); break;
            case ',': lex_push_tok_adv(l, TOKEN_KIND_COMMA); break;
            case '=': lex_push_tok_adv_cond(l, '=', TOKEN_KIND_DOUBLE_EQUAL, TOKEN_KIND_EQUAL); break;
            case '!': lex_push_tok_adv_cond(l, '=', TOKEN_KIND_BANG_EQUAL, TOKEN_KIND_BANG); break;
            case '<': lex_push_tok_adv_cond(l, '=', TOKEN_KIND_LANGBR_EQUAL, TOKEN_KIND_LANGBR); break;
            case '>': lex_push_tok_adv_cond(l, '=', TOKEN_KIND_RANGBR_EQUAL, TOKEN_KIND_RANGBR); break;
            case '&': lex_push_tok_adv_cond(l, '&', TOKEN_KIND_DOUBLE_AMP, TOKEN_KIND_AMP); break;
            case '+': lex_push_tok_adv(l, TOKEN_KIND_PLUS); break;
            case '-': lex_push_tok_adv(l, TOKEN_KIND_MINUS); break;
            case '*': lex_push_tok_adv(l, TOKEN_KIND_STAR); break;
            case '/': {
                if (*(l->current+1) == '/') {
                    while (*l->current != '\n' && *l->current != '\0')
                        l->current++;
                }
                else {
                    lex_push_tok_adv(l, TOKEN_KIND_FSLASH);
                }
            } break;

            default: {
                if (*l->current >= 0 && 
                    !ascii_error_table[(size_t)*l->current]) {
                    lex_error_from_current(
                            l,
                            "invalid character `{}`",
                            *l->current);
                    ascii_error_table[(size_t)*l->current] = true;
                }
                l->current++;
            } break;
        }
    }
}

