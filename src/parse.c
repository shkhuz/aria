#include "parse.h"
#include "printf.h"
#include "buf.h"
#include "msg.h"

ParseContext parse_new_context(Srcfile* srcfile) {
    ParseContext p;
    p.srcfile = srcfile;
    p.token_idx = 0;
    p.error = false;
    return p;
}

static Token* current(ParseContext* p) {
    if (p->token_idx < buflen(p->srcfile->tokens)) {
        return p->srcfile->tokens[p->token_idx];
    }
    assert(0);
    return NULL;
}

static Token* next(ParseContext* p) {
    if (p->token_idx+1 < buflen(p->srcfile->tokens)) {
        return p->srcfile->tokens[p->token_idx+1];
    }
    assert(0);
    return NULL;
}

static Token* previous(ParseContext* p) {
    if (p->token_idx > 0) {
        return p->srcfile->tokens[p->token_idx-1];
    }
    assert(0);
    return NULL;
}

static void goto_next_tok(ParseContext* p) {
    if (p->token_idx < buflen(p->srcfile->tokens)) {
        p->token_idx++;
    }
}

static void goto_prev_tok(ParseContext* p) {
    if (p->token_idx > 0) {
        p->token_idx--;
    }
}

static void check_eof(ParseContext* p, Token* pair) {
    if (current(p)->kind == TOKEN_KIND_EOF) {
        note_tok(pair, "while matching `%to`...", pair);
        fatal_error_tok(current(p), "unexpected end of file");
    }
}

void parse(ParseContext* p) {
    while (current(p)->kind != TOKEN_KIND_EOF) {
        goto_next_tok(p);
        check_eof(p, p->srcfile->tokens[0]);
    }
}
