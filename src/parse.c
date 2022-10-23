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

static bool match(ParseContext* p, TokenKind kind) {
    if (current(p)->kind == kind) {
        goto_next_tok(p);
        return true;
    }
    return false;
}

static bool match_keyword(ParseContext* p, const char* keyword) {
    usize len = strlen(keyword);
    Token* tok = current(p);
    if (tok->kind == TOKEN_KIND_KEYWORD &&
        tok->ch_count == len &&
        strncmp(tok->start, keyword, len) == 0) {
        goto_next_tok(p);
        return true;
    }
    return false;
}

static Token* expect_keyword(ParseContext* p, const char* keyword) {
    if (match_keyword(p, keyword)) {
        return previous(p);
    }
    fatal_error_tok(
        current(p),
        "expected keyword `%s`, got `%to`",
        keyword,
        current(p));
    return NULL;
}

static Token* expect(ParseContext* p, TokenKind kind, const char* expected) {
    if (!match(p, kind)) {
        fatal_error_tok(
            current(p),
            "expected %s, got `%to`",
            expected,
            current(p));
        return NULL;
    }
    return previous(p);
}

static Token* expect_identifier(ParseContext* p, const char* expected) {
    return expect(p, TOKEN_KIND_IDENTIFIER, expected);
}

static Token* expect_lparen(ParseContext* p) {
    return expect(p, TOKEN_KIND_LPAREN, "`(`");
}

static Token* expect_lbrace(ParseContext* p) {
    return expect(p, TOKEN_KIND_LBRACE, "`{`");
}

static Token* expect_colon(ParseContext* p) {
    return expect(p, TOKEN_KIND_COLON, "`:`");
}

static Token* expect_semicolon(ParseContext* p) {
    return expect(p, TOKEN_KIND_SEMICOLON, "`;`");
}

static Token* expect_comma(ParseContext* p) {
    return expect(p, TOKEN_KIND_COMMA, "`,`");
}

void parse(ParseContext* p) {
    /*
    while (current(p)->kind != TOKEN_KIND_EOF) {
        goto_next_tok(p);
        check_eof(p, p->srcfile->tokens[0]);
    }
    */
    Token* first = current(p);
    goto_next_tok(p);
    goto_next_tok(p);
    msg(
        MSG_KIND_ERROR,
        current(p)->srcfile,
        first->line,
        first->col,
        current(p)->end - first->start,
        "error hahahaha");
}
