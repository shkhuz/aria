#include "lex.h"
#include "printf.h"
#include "buf.h"

LexContext lex_new_context(Srcfile* srcfile) {
    LexContext l;
    l.srcfile = srcfile;
    l.srcfile->tokens = NULL;
    l.start = srcfile->handle.contents;
    l.current = l.start;
    l.last_newl = l.start;
    l.line = 1;
    l.error = false;
    return l;
}

static usize get_col(LexContext* l, const char* c) {
    usize col = (usize)(c - l->last_newl);
    if (l->line == 1) col++;
    return col;
}

static usize get_start_col(LexContext* l) {
    return get_col(l, l->start);
}

static usize get_current_col(LexContext* l) {
    return get_col(l, l->current);
}

static void push_tok(LexContext* l, TokenKind kind) {
    Token* t = malloc(sizeof(Token));
    t->kind = kind;
    t->start = l->start;
    t->end = l->current;
    t->srcfile = l->srcfile;
    t->line = l->line;
    t->col = get_start_col(l);
    t->ch_count = (usize)(l->current - l->start);
    bufpush(l->srcfile->tokens, t);
}

static void push_tok_adv_one(LexContext* l, TokenKind kind) {
    l->current++;
    push_tok(l, kind);
}

void lex(LexContext* l) {
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

            case ' ':
            case '\r': {
                l->current++;
            } break;

            case '\0': {
                push_tok_adv_one(l, TOKEN_KIND_EOF);
            } return;

            default: {
                l->current++;
            } break;
        }
    }
}
