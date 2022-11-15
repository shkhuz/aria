#include "parse.h"
#include "printf.h"
#include "buf.h"
#include "msg.h"

ParseCtx parse_new_context(Srcfile* srcfile) {
    ParseCtx p;
    p.srcfile = srcfile;
    p.srcfile->stmts = NULL;
    p.token_idx = 0;
    p.error = false;
    return p;
}

static Token* current(ParseCtx* p) {
    if (p->token_idx < buflen(p->srcfile->tokens)) {
        return p->srcfile->tokens[p->token_idx];
    }
    assert(0);
    return NULL;
}

static Token* next(ParseCtx* p) {
    if (p->token_idx+1 < buflen(p->srcfile->tokens)) {
        return p->srcfile->tokens[p->token_idx+1];
    }
    assert(0);
    return NULL;
}

static Token* previous(ParseCtx* p) {
    if (p->token_idx > 0) {
        return p->srcfile->tokens[p->token_idx-1];
    }
    assert(0);
    return NULL;
}

static void goto_next_tok(ParseCtx* p) {
    if (p->token_idx < buflen(p->srcfile->tokens)) {
        p->token_idx++;
    }
}

static void goto_prev_tok(ParseCtx* p) {
    if (p->token_idx > 0) {
        p->token_idx--;
    }
}

static void check_eof(ParseCtx* p, Token* pair) {
    if (current(p)->kind == TOKEN_KIND_EOF) {
        note_tok(pair, "while matching `%to`...", pair);
        fatal_error_tok(current(p), "unexpected end of file");
    }
}

static bool match(ParseCtx* p, TokenKind kind) {
    if (current(p)->kind == kind) {
        goto_next_tok(p);
        return true;
    }
    return false;
}

static bool match_keyword(ParseCtx* p, const char* keyword) {
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

static Token* expect_keyword(ParseCtx* p, const char* keyword) {
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

static Token* expect(ParseCtx* p, TokenKind kind, const char* expected) {
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

static Token* expect_identifier(ParseCtx* p, const char* expected) {
    return expect(p, TOKEN_KIND_IDENTIFIER, expected);
}

static Token* expect_lparen(ParseCtx* p) {
    return expect(p, TOKEN_KIND_LPAREN, "`(`");
}

static Token* expect_lbrace(ParseCtx* p) {
    return expect(p, TOKEN_KIND_LBRACE, "`{`");
}

static Token* expect_colon(ParseCtx* p) {
    return expect(p, TOKEN_KIND_COLON, "`:`");
}

static Token* expect_semicolon(ParseCtx* p) {
    return expect(p, TOKEN_KIND_SEMICOLON, "`;`");
}

static Token* expect_comma(ParseCtx* p) {
    return expect(p, TOKEN_KIND_COMMA, "`,`");
}

/*
static FunctionHeader parse_function_header(ParseCtx* p) {
    Token* identifier = expect_identifier(p, "function name");
    Token* lparen = expect_lparen(p);

    Stmt** params = NULL;
    while (!match(p, TOKEN_KIND_RPAREN)) {
        check_eof(p, lparen);
        Token* param_identifier = expect_identifier(p, "parameter name");
        expect_colon(p);
        Expr* param_type = parse_expr_type(p);
        bufpush(params, stmt_param_new(param_identifier, param_type));
        if (current(p)->kind != TOKEN_KIND_RPAREN) {
            expect_comma(p);
        }
    }

    Expr* ret_type = type_placeholders.void_type;
    if (current(p)->kind != TOKEN_KIND_LBRACE) {
        ret_type = parse_expr_type(p);
    }
    return function_header_new(identifier, params, ret_type);
}

static Stmt* parse_root_stmt(ParseCtx* p, bool error_on_no_match) {
    if (match_keyword(p, "fn")) {
        FunctionHeader header = parse_function_header(p);
        Token* lbrace = expect_lbrace(p);
        Expr* body = parse_scoped_block(p, lbrace);
        return stmt_function_def_new(header, body);
    }
}
*/

void parse(ParseCtx* p) {
    /*
    while (current(p)->kind != TOKEN_KIND_EOF) {
        Stmt* stmt = root_stmt(p, true);
        if (stmt) bufpush(p->srcfile->stmts, stmt);
    }
    */
    /*
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
    */
}
