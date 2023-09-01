#include "lex.h"
#include "buf.h"
#include "msg.h"
#include "compile.h"

LexCtx lex_new_context(Srcfile* srcfile, CompileCtx* compile_ctx, jmp_buf* error_handler_pos) {
    LexCtx l;
    l.srcfile = srcfile;
    l.srcfile->tokens = NULL;
    l.start = srcfile->handle.contents;
    l.current = l.start;
    l.last_newl = l.start;
    l.error = false;
    l.compile_ctx = compile_ctx;
    l.error_handler_pos = error_handler_pos;
    memset(l.ascii_error_table, 0, 128);
    l.invalid_char_error = false;
    return l;
}

static inline void msg_emit(LexCtx* l, Msg* msg) {
    _msg_emit(msg, l->compile_ctx);
    if (msg->kind == MSG_ERROR) l->error = true;
}

static inline void fatal_msg_emit(LexCtx* l, Msg* msg) {
    _msg_emit(msg, l->compile_ctx);
    if (msg->kind == MSG_ERROR) {
        l->error = true;
        longjmp(*l->error_handler_pos, 1);
    }
}

static Span span_from_start_to_current(LexCtx* l) {
    return span_new(
        l->srcfile,
        l->start - l->srcfile->handle.contents,
        l->current - l->srcfile->handle.contents);
}

static Span span_to_current_from(LexCtx* l, const char* from) {
    return span_new(
        l->srcfile,
        from - l->srcfile->handle.contents,
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

static Token* last_tok(LexCtx* l) {
    return l->srcfile->tokens[buflen(l->srcfile->tokens)-1];
}

static inline int char_to_digit(char c) {
    return c - 48;
}

static char lex_escaped_char(LexCtx* l) {
    const char* pos = l->current-1;
    char c = *l->current;
    l->current++;
    switch (c) {
        case '\'': case '"': case '\\':
            return c;
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'v': return '\v';
    }
    Msg msg = msg_with_span(
        MSG_ERROR,
        format_string("unknown escape character: '\\%c'", c),
        span_to_current_from(l, pos));
    fatal_msg_emit(l, &msg);
    return '\0';
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
                while (isdigit(*l->current) || *l->current == '_') {
                    if (*l->current == '_' && !isdigit(*(l->current+1))) break;
                    l->current++;
                }
                push_tok(l, TOKEN_INTEGER_LITERAL);
            } break;

            case '\"': {
                bool error = false;
                char* str = NULL;
                l->current++;
                while (*l->current != '\"') {
                    if (*l->current == '\n' || *l->current == '\0') {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            "unterminated string literal",
                            span_from_start_to_current(l));
                        fatal_msg_emit(l, &msg);
                    }

                    if (*l->current == '\\') {
                        l->current++;
                        char c = lex_escaped_char(l);
                        bufpush(str, c);
                    } else {
                        bufpush(str, *l->current);
                        l->current++;
                    }
                }
                bufpush(str, '\0');

                if (!error) {
                    l->current++;
                    push_tok(l, TOKEN_STRING_LITERAL);
                    last_tok(l)->str = str;
                }
            } break;

            case '\'': {
                l->current++;
                char c = *l->current;
                l->current++;
                char final = (c == '\\') ? lex_escaped_char(l) : c;

                c = *l->current;
                if (c != '\'') {
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        "unterminated char literal",
                        span_from_start_to_current(l));
                    fatal_msg_emit(l, &msg);
                }
                l->current++;
                push_tok(l, TOKEN_CHAR_LITERAL);
                last_tok(l)->c = final;
            } break;

            case '{': push_tok_adv(l, TOKEN_LBRACE); break;
            case '}': push_tok_adv(l, TOKEN_RBRACE); break;
            case '[': push_tok_adv(l, TOKEN_LBRACK); break;
            case ']': push_tok_adv(l, TOKEN_RBRACK); break;
            case '(': push_tok_adv(l, TOKEN_LPAREN); break;
            case ')': push_tok_adv(l, TOKEN_RPAREN); break;
            case ';': push_tok_adv(l, TOKEN_SEMICOLON); break;
            case '.': push_tok_adv(l, TOKEN_DOT); break;
            case ',': push_tok_adv(l, TOKEN_COMMA); break;
            case ':': push_tok_adv_cond(l, ':', TOKEN_DOUBLE_COLON, TOKEN_COLON); break;
            case '=': push_tok_adv_cond(l, '=', TOKEN_DOUBLE_EQUAL, TOKEN_EQUAL); break;
            case '!': push_tok_adv_cond(l, '=', TOKEN_BANG_EQUAL, TOKEN_BANG); break;
            case '<': push_tok_adv_cond(l, '=', TOKEN_LANGBR_EQUAL, TOKEN_LANGBR); break;
            case '>': push_tok_adv_cond(l, '=', TOKEN_RANGBR_EQUAL, TOKEN_RANGBR); break;
            case '&': push_tok_adv_cond(l, '&', TOKEN_DOUBLE_AMP, TOKEN_AMP); break;
            case '%': push_tok_adv_cond(l, '=', TOKEN_PERC_EQUAL, TOKEN_PERC); break;
            case '+': push_tok_adv_cond(l, '=', TOKEN_PLUS_EQUAL, TOKEN_PLUS); break;
            case '-': push_tok_adv_cond(l, '=', TOKEN_MINUS_EQUAL, TOKEN_MINUS); break;
            case '*': push_tok_adv_cond(l, '=', TOKEN_STAR_EQUAL, TOKEN_STAR); break;
            case '@': push_tok_adv(l, TOKEN_AT); break;
            case '$': push_tok_adv(l, TOKEN_DOLLAR); break;

            case '/': {
                if (*(l->current+1) == '/') {
                    while (*l->current != '\n' && *l->current != '\0') l->current++;
                } else {
                    push_tok_adv_cond(l, '=', TOKEN_FSLASH_EQUAL, TOKEN_FSLASH);
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
