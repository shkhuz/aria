#include "lex.h"
#include "buf.h"
#include "stri.h"
#include "msg.h"

static bool ascii_errored_table[128];

static void _vlex_error(
        LexContext* l,
        size_t column,
        size_t char_count,
        const char* fmt,
        va_list ap);
static void lex_error_from_start(LexContext* l, const char* fmt, ...);
static void lex_error_from_current(LexContext* l, const char* fmt, ...);
static void lex_push_tok(LexContext* l, TokenKind kind);
static void lex_push_tok_adv(LexContext* l, TokenKind kind);
static void lex_push_tok_adv_cond(
        LexContext* l, 
        char c, 
        TokenKind matched,
        TokenKind not_matched);
static size_t lex_get_column(LexContext* l, char* c);
static size_t lex_get_column_from_start(LexContext* l);
static size_t lex_get_column_from_current(LexContext* l);

void lex(LexContext* l) {
    l->srcfile->tokens = null;
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
                for (size_t i = 0; i < buf_len(aria_keywords); i++) {
                    /* if (stri(aria_keywords[i]) == strni(l->start, l->current)) { */
                    if (strncmp(aria_keywords[i], l->start, strlen(aria_keywords[i])) == 0) {
                        kind = TOKEN_KIND_KEYWORD;
                        break;
                    }
                }
                lex_push_tok(l, kind);
            } break;

            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9': {
                // TODO: refactor these out so that they are
                // not allocated every time. Constants can be allocated
                // once.
                ALLOC_WITH_TYPE(base, bigint);
                bigint_init_u64(base, 10);
                ALLOC_WITH_TYPE(digit_bi, bigint);
                bigint_init(digit_bi);

                ALLOC_WITH_TYPE(val, bigint);
                bigint_init(val);

                while (isdigit(*l->current) || *l->current == '_') {
                    if (*l->current == '_' && !isdigit(*(l->current+1))) {
                        break;
                    }

                    if (*l->current != '_') {
                        int digit = char_to_digit(*l->current);
                        bigint_init_u64(digit_bi, (u64)digit);
                        bigint_mul(val, base, val);
                        bigint_add(val, digit_bi, val);
                        bigint_clear(digit_bi);
                    }
                    l->current++;
                }

                lex_push_tok(l, TOKEN_KIND_INTEGER);
                l->srcfile->tokens[buf_len(l->srcfile->tokens)-1]
                    ->integer.val = val;

                bigint_clear(base);
                free(base);
                free(digit_bi);
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
                lex_push_tok_adv(l, TOKEN_KIND_EOF);
            } return;

            case '{': lex_push_tok_adv(l, TOKEN_KIND_LBRACE); break;
            case '}': lex_push_tok_adv(l, TOKEN_KIND_RBRACE); break;
            case '(': lex_push_tok_adv(l, TOKEN_KIND_LPAREN); break;
            case ')': lex_push_tok_adv(l, TOKEN_KIND_RPAREN); break;
            case ':': lex_push_tok_adv(l, TOKEN_KIND_COLON); break;
            case ';': lex_push_tok_adv(l, TOKEN_KIND_SEMICOLON); break;
            case ',': lex_push_tok_adv(l, TOKEN_KIND_COMMA); break;
            case '=': lex_push_tok_adv_cond(l, '=', TOKEN_KIND_DOUBLE_EQUAL, TOKEN_KIND_EQUAL); break;
            case '!': lex_push_tok_adv_cond(l, '=', TOKEN_KIND_BANG_EQUAL, TOKEN_KIND_BANG); break;
            case '<': lex_push_tok_adv_cond(l, '=', TOKEN_KIND_LANGBR_EQUAL, TOKEN_KIND_LANGBR); break;
            case '>': lex_push_tok_adv_cond(l, '=', TOKEN_KIND_RANGBR_EQUAL, TOKEN_KIND_RANGBR); break;
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
                    !ascii_errored_table[(size_t)*l->current]) {
                    lex_error_from_current(
                            l,
                            "invalid character `%c`",
                            *l->current);
                    ascii_errored_table[(size_t)*l->current] = true;
                }
                l->current++;
            } break;
        }
    }
}

void lex_push_tok(LexContext* l, TokenKind kind) {
    ALLOC_WITH_TYPE(token, Token);
    token->kind = kind;
    token->lexeme = kind == TOKEN_KIND_EOF ? "EOF" : strni(l->start, l->current);
    token->start = l->start;
    token->end = l->current;
    token->srcfile = l->srcfile;
    token->line = l->line;
    token->column = lex_get_column_from_start(l);
    token->char_count = l->current - l->start;
    buf_push(l->srcfile->tokens, token);
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

size_t lex_get_column(LexContext* l, char* c) {
    size_t column = c - l->last_newline;
    if (l->line == 1) 
        column++;
    return column;
}

size_t lex_get_column_from_start(LexContext* l) {
    return lex_get_column(l, l->start);
}

size_t lex_get_column_from_current(LexContext* l) {
    return lex_get_column(l, l->current);
}

void _vlex_error(
        LexContext* l,
        size_t column,
        size_t char_count,
        const char* fmt,
        va_list ap) {
    l->error = true;
    vdefault_msg(
            MSG_KIND_ERROR,
            l->srcfile,
            l->line,
            column,
            char_count,
            fmt,
            ap);
    // TODO: should we terminate compilation or continue?
}

void lex_error_from_start(LexContext* l, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _vlex_error(
            l,
            lex_get_column_from_start(l),
            l->current - l->start,
            fmt,
            ap);
    va_end(ap);
}

void lex_error_from_current(LexContext* l, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _vlex_error(
            l,
            lex_get_column_from_current(l),
            1,
            fmt,
            ap);
    va_end(ap);
}
