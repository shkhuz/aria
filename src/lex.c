#include "lex.h"
#include "srcfile.h"
#include "msg.h"
#include "token.h"

LexCtx lexctx_new(
    struct Srcfile* src,
    struct CompileCtx* compilectx, 
    jmp_buf* error_handler_pos
) {
    LexCtx l;
    l.src = src;
    l.src->tokens = NULL;
    // Index 0 is a placeholder
    // for error/empty tokens.
    bufpush(l.src->tokens, (Token){});
    l.start = src->handle.contents;
    l.current = l.start;
    l.lastnl = l.start;
    l.error = false;
    l.compilectx = compilectx;
    l.error_handler_pos = error_handler_pos;
    memset(l.ascii_error_table, 0, 128);
    l.invalid_char_error = false;
    return l;
}

#define msg_with_span(kind, msg, span) _msg_with_span(kind, msg, span, l->src)
#define msg_addl_fat(m, msg, span) _msg_addl_fat(m, msg, span, l->src)

static inline void msg_emit(LexCtx* l, Msg* msg) {
    _msg_emit(msg, l->compilectx);
    if (msg->kind == MSG_ERROR) l->error = true;
}

static inline void fatal_msg_emit(LexCtx* l, Msg* msg) {
    _msg_emit(msg, l->compilectx);
    if (msg->kind == MSG_ERROR) {
        l->error = true;
        longjmp(*l->error_handler_pos, 1);
    }
}

static inline Span span_from_start_to_current(LexCtx* l) {
    return span_new(
        l->start - l->src->handle.contents,
        l->current - l->src->handle.contents
    );
}

static inline Span span_to_current_from(LexCtx* l, const char* from) {
    return span_new(
        from - l->src->handle.contents,
        l->current - l->src->handle.contents
    );
}

static bool match(LexCtx* l, char c) {
    if (*l->current == c) {
        l->current++;
        return true;
    }
    return false;
}

static inline char peek(LexCtx* l) {
    return *(l->current+1); 
}

static void push_tok(LexCtx* l, TokenKind kind) {
    Token t = token_new(kind, span_from_start_to_current(l));
    bufpush(l->src->tokens, t);
}

static inline void push_tok_adv(LexCtx* l, TokenKind kind) {   
    l->current++;
    push_tok(l, kind);
}

static void push_tok_adv_cond(
    LexCtx* l, 
    char c, 
    TokenKind match, 
    TokenKind orelse
) {
    l->current++;
    if (*l->current == c) push_tok_adv(l, match);
    else push_tok(l, orelse);
}

static inline Token* last_tok(LexCtx* l) {
    return &l->src->tokens[buflen(l->src->tokens)-1];
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
                TokenKind kind = TK_IDENT;
                while (isalnum(*l->current) || *l->current == '_')
                    l->current++;

                for (int i = 0; i < KEYWORDS_LEN; i++) {
                    if (slice_eql_to_str(
                        l->start, 
                        l->current-l->start, 
                        keywords[i].k
                    )) {
                        kind = keywords[i].v;
                        break;
                    }
                }
                push_tok(l, kind);
            } break;

            case ':': push_tok_adv(l, TK_COLON); break;
            case ',': push_tok_adv(l, TK_COMMA); break;
            case '=': push_tok_adv(l, TK_EQUAL); break;
            case '{': push_tok_adv(l, TK_LBRACE); break;
            case '(': push_tok_adv(l, TK_LPAREN); break;
            case '}': push_tok_adv(l, TK_RBRACE); break;
            case ')': push_tok_adv(l, TK_RPAREN); break;
            case ';': push_tok_adv(l, TK_SEMICOLON); break;

            case '\"': {
                char* str = NULL;
                l->current++;
                while (*l->current != '\"') {
                    if (*l->current == '\n' || *l->current == '\0') {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            "unterminated string literal",
                            span_from_start_to_current(l)
                        );
                        fatal_msg_emit(l, &msg);
                    }

                    if (*l->current == '\\') {
                        l->current++;
                        // unsigned char c = escape_char(l);
                        // bufpush(str, c);
                    } else {
                        bufpush(str, *l->current);
                        l->current++;
                    }
                }
                bufpush(str, '\0');
                bufpush(token_strlit_data, (StrlitData){ str, buflen(str)-1 });

                push_tok_adv(l, TK_STRLIT);
                last_tok(l)->extra = buflen(token_strlit_data)-1;
            } break;

            case '/': {
                if (peek(l) == '/') {
                    while (*l->current != '\n' && *l->current != '\0') 
                        l->current++;
                }
            } break;

            case ' ':
            case '\t':
            case '\r': {
                l->current++;
            } break;

            case '\n': {
                l->lastnl = l->current;
                l->current++;
            } break;

            case '\0': {
                push_tok_adv(l, TK_EOF);
                if (l->invalid_char_error) {
                    Msg msg = msg_with_no_span(
                        MSG_NOTE,
                        "each invalid character is reported only once"
                    );
                    msg_emit(l, &msg);
                }
            } return;

            default: {
                char c = *l->current;
                l->current++;
                if (c >= 0 && !l->ascii_error_table[(unsigned)c]) {
                    // TODO: Don't print the char in the fmt string. 
                    // Instead print the unicode identifier.
                    l->ascii_error_table[(unsigned)c] = true;
                    l->invalid_char_error = true;
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        "invalid character",
                        span_from_start_to_current(l)
                    );
                    msg_emit(l, &msg);
                }
            } break;
        }
    }
}
