#include "lex.h"
#include "printf.h"
#include "buf.h"
#include "msg.h"

LexCtx lex_new_context(Srcfile* srcfile) {
    LexCtx l;
    l.srcfile = srcfile;
    l.srcfile->tokens = NULL;
    l.start = srcfile->handle.contents;
    l.current = l.start;
    l.last_newl = l.start;
    l.line = 1;
    l.error = false;
    memset(l.ascii_error_table, 0, 128);
    l.invalid_char_error = false;
    return l;
}

static usize get_col(LexCtx* l, const char* c) {
    usize col = (usize)(c - l->last_newl);
    if (l->line == 1) col++;
    return col;
}

static usize get_start_col(LexCtx* l) {
    return get_col(l, l->start);
}

static usize get_current_col(LexCtx* l) {
    return get_col(l, l->current);
}

static void push_tok(LexCtx* l, TokenKind kind) {
    Token* t = token_new(
        kind,
        l->start,
        l->current,
        l->srcfile,
        l->line,
        get_start_col(l),
        (usize)(l->current - l->start));
    bufpush(l->srcfile->tokens, t);
}

static void push_tok_adv(LexCtx* l, TokenKind kind) {
    l->current++;
    push_tok(l, kind);
}

static void push_tok_adv_cond(LexCtx* l, char c, TokenKind matched, TokenKind not_matched) {
    if (*(l->current+1) == c) {
        l->current++;
        push_tok_adv(l, matched);
    } else {
        push_tok_adv(l, not_matched);
    }
}

static void error(LexCtx* l, usize col, usize ch_count, const char* fmt, va_list args) {
    l->error = true;
    vmsg(
        MSG_KIND_ERROR,
        l->srcfile,
        l->line,
        col,
        ch_count,
        fmt,
        args);
}

static void error_from_start(LexCtx* l, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    error(l, get_start_col(l), l->current - l->start, fmt, args);
    va_end(args);
}

static void error_from_current(LexCtx* l, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    error(l, get_current_col(l), 1, fmt, args);
    va_end(args);
}

static inline int char_to_digit(char c) {
    return c - 48;
}

void lex(LexCtx* l) {
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
                while (isalnum(*l->current) || *l->current == '_') {
                    l->current++;
                }

                bufloop(keywords, i) {
                    if (keywords[i].v == (l->current-l->start) &&
                        strncmp(l->start, keywords[i].k, keywords[i].v) == 0) {
                        kind = TOKEN_KIND_KEYWORD;
                        break;
                    }
                }
                push_tok(l, kind);
            } break;

            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9': {
                // TODO: move these so that we don't initialize them every time.
                bigint base;
                bigint_init_u64(&base, 10);
                bigint val;
                bigint_init(&val);
                
                while (isdigit(*l->current) || *l->current == '_') {
                    if (*l->current == '_' && !isdigit(*(l->current+1))) break;

                    if (*l->current != '_') {
                        int digit = char_to_digit(*l->current);
                        bigint digitbi;
                        bigint_init_u64(&digitbi, (u64)digit);

                        bigint_mul(&val, &base, &val);
                        bigint_add(&val, &digitbi, &val);
                    }
                    l->current++;
                }

                push_tok(l, TOKEN_KIND_INTEGER_LITERAL);
                // We don't explicitly deep copy the `val` struct because
                // it isn't used after this.
                l->srcfile->tokens[buflen(l->srcfile->tokens)-1]->intg.val = val;
            } break;

            case '\"': {
                l->current++;
                while (*l->current != '\"') {
                    if (*l->current == '\n' || *l->current == '\0') {
                        error_from_start(l, "unterminated string literal");
                        l->current++;
                        break;
                    }
                    l->current++;
                }

                Token* t = token_new(
                    TOKEN_KIND_STRING,
                    l->start,
                    l->current+1,
                    l->srcfile,
                    l->line,
                    get_start_col(l),
                    (usize)(l->current+1 - l->start));
                bufpush(l->srcfile->tokens, t);
                l->current++;
            } break;

            case '{': push_tok_adv(l, TOKEN_KIND_LBRACE); break;
            case '}': push_tok_adv(l, TOKEN_KIND_RBRACE); break;
            case '[': push_tok_adv(l, TOKEN_KIND_LBRACK); break;
            case ']': push_tok_adv(l, TOKEN_KIND_RBRACK); break;
            case '(': push_tok_adv(l, TOKEN_KIND_LPAREN); break;
            case ')': push_tok_adv(l, TOKEN_KIND_RPAREN); break;
            case ':': push_tok_adv(l, TOKEN_KIND_COLON); break;
            case ';': push_tok_adv(l, TOKEN_KIND_SEMICOLON); break;
            case '.': push_tok_adv(l, TOKEN_KIND_DOT); break;
            case ',': push_tok_adv(l, TOKEN_KIND_COMMA); break;
            case '=': push_tok_adv_cond(l, '=', TOKEN_KIND_DOUBLE_EQUAL, TOKEN_KIND_EQUAL); break;
            case '!': push_tok_adv_cond(l, '=', TOKEN_KIND_BANG_EQUAL, TOKEN_KIND_BANG); break;
            case '<': push_tok_adv_cond(l, '=', TOKEN_KIND_LANGBR_EQUAL, TOKEN_KIND_LANGBR); break;
            case '>': push_tok_adv_cond(l, '=', TOKEN_KIND_RANGBR_EQUAL, TOKEN_KIND_RANGBR); break;
            case '&': push_tok_adv_cond(l, '&', TOKEN_KIND_DOUBLE_AMP, TOKEN_KIND_AMP); break;
            case '+': push_tok_adv(l, TOKEN_KIND_PLUS); break;
            case '-': push_tok_adv(l, TOKEN_KIND_MINUS); break;
            case '*': push_tok_adv(l, TOKEN_KIND_STAR); break;

            case '/': {
                if (*(l->current+1) == '/') {
                    while (*l->current != '\n' && *l->current != '\0') l->current++;
                } else {
                    push_tok_adv(l, TOKEN_KIND_FSLASH);
                }
            } break;

            case ' ':
            case '\t':
            case '\r': {
                l->current++;
            } break;

            case '\n': {
                l->last_newl = l->current;
                l->current++;
                l->line++;
            } break;

            case '\0': {
                push_tok_adv(l, TOKEN_KIND_EOF);
                if (l->invalid_char_error) {
                    root_note("each invalid character is reported only once");
                }
            } return;

            default: {
                if (*l->current >= 0 && !l->ascii_error_table[(usize)*l->current]) {
                    // Don't print the char in the fmt string. Instead print the
                    // unicode identifier.
                    error_from_current(l, "invalid character `%c`", *l->current);
                    l->ascii_error_table[(usize)*l->current] = true;
                    l->invalid_char_error = true;
                }
                l->current++;
            } break;
        }
    }
}
