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
    l.error = false;
    memset(l.ascii_error_table, 0, 128);
    l.invalid_char_error = false;
    return l;
}

static Span span_from_start_to_current(LexCtx* l) {
    return span_new(
        l->srcfile,
        l->start - l->srcfile->handle.contents,
        l->current - l->srcfile->handle.contents);
}

static void push_tok(LexCtx* l, TokenKind kind) {
    Token* t = token_new(kind, span_from_start_to_current(l));
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

static inline int char_to_digit(char c) {
    return c - 48;
}

static inline void msg_emit(LexCtx* l, Msg* msg) {
    if (msg->kind == MSG_ERROR) l->error = true;
    _msg_emit(msg);
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
                TokenKind kind = TOKEN_IDENTIFIER;
                while (isalnum(*l->current) || *l->current == '_') {
                    l->current++;
                }

                bufloop(keywords, i) {
                    if (slice_eql_to_str(l->start, l->current-l->start, keywords[i].k)) {
                        kind = keywords[i].v;
                        break;
                    }
                }
                push_tok(l, kind);
            } break;

            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9': {
                /*
                // TODO: move these so that we don't initialize them every time.
                bigint base;
                bigint_init_u64(&base, 10);
                bigint val;
                bigint_init(&val);
                */
                
                while (isdigit(*l->current) || *l->current == '_') {
                    if (*l->current == '_' && !isdigit(*(l->current+1))) break;

                    /*
                    if (*l->current != '_') {
                        int digit = char_to_digit(*l->current);
                        bigint digitbi;
                        bigint_init_u64(&digitbi, (u64)digit);

                        bigint_mul(&val, &base, &val);
                        bigint_add(&val, &digitbi, &val);
                    }
                    */
                    l->current++;
                }
                push_tok(l, TOKEN_INTEGER_LITERAL);
            } break;

            case '\"': {
                l->current++;
                while (*l->current != '\"') {
                    if (*l->current == '\n' || *l->current == '\0') {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            "unterminated string literal",
                            span_from_start_to_current(l));
                        msg_emit(l, &msg);
                        l->current++;
                        break;
                    }
                    l->current++;
                }

                l->current++;
                Token* t = token_new(TOKEN_STRING, span_from_start_to_current(l));
                bufpush(l->srcfile->tokens, t);
            } break;

            case '{': push_tok_adv(l, TOKEN_LBRACE); break;
            case '}': push_tok_adv(l, TOKEN_RBRACE); break;
            case '[': push_tok_adv(l, TOKEN_LBRACK); break;
            case ']': push_tok_adv(l, TOKEN_RBRACK); break;
            case '(': push_tok_adv(l, TOKEN_LPAREN); break;
            case ')': push_tok_adv(l, TOKEN_RPAREN); break;
            case ':': push_tok_adv(l, TOKEN_COLON); break;
            case ';': push_tok_adv(l, TOKEN_SEMICOLON); break;
            case '.': push_tok_adv(l, TOKEN_DOT); break;
            case ',': push_tok_adv(l, TOKEN_COMMA); break;
            case '=': push_tok_adv_cond(l, '=', TOKEN_DOUBLE_EQUAL, TOKEN_EQUAL); break;
            case '!': push_tok_adv_cond(l, '=', TOKEN_BANG_EQUAL, TOKEN_BANG); break;
            case '<': push_tok_adv_cond(l, '=', TOKEN_LANGBR_EQUAL, TOKEN_LANGBR); break;
            case '>': push_tok_adv_cond(l, '=', TOKEN_RANGBR_EQUAL, TOKEN_RANGBR); break;
            case '&': push_tok_adv_cond(l, '&', TOKEN_DOUBLE_AMP, TOKEN_AMP); break;
            case '+': push_tok_adv(l, TOKEN_PLUS); break;
            case '-': push_tok_adv(l, TOKEN_MINUS); break;
            case '*': push_tok_adv(l, TOKEN_STAR); break;

            case '/': {
                if (*(l->current+1) == '/') {
                    while (*l->current != '\n' && *l->current != '\0') l->current++;
                } else {
                    push_tok_adv(l, TOKEN_FSLASH);
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
            } break;

            case '\0': {
                push_tok_adv(l, TOKEN_EOF);
                if (l->invalid_char_error) {
                    Msg msg = msg_with_no_span(
                        MSG_NOTE,
                        "each invalid character is reported only once");
                    msg_emit(l, &msg);
                }
            } return;

            default: {
                char c = *l->current;
                l->current++;
                if (c >= 0 && !l->ascii_error_table[(unsigned)c]) {
                    // Don't print the char in the fmt string. Instead print the
                    // unicode identifier.
                    l->ascii_error_table[(unsigned)c] = true;
                    l->invalid_char_error = true;
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        "invalid character",
                        span_from_start_to_current(l));
                    msg_emit(l, &msg);
                }
            } break;
        }
    }
}
