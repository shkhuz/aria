#include "parse.h"
#include "printf.h"
#include "buf.h"
#include "msg.h"

static AstNode* parse_root(ParseCtx* p, bool error_on_no_match);

ParseCtx parse_new_context(Srcfile* srcfile) {
    ParseCtx p;
    p.srcfile = srcfile;
    p.srcfile->astnodes = NULL;
    p.token_idx = 0;
    p.error = false;
    return p;
}

static inline void msg_emit(ParseCtx* p, Msg* msg) {
    if (msg->kind == MSG_KIND_ERROR) p->error = true;
    _msg_emit(msg);
    terminate_compilation();
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
        Msg msg = msg_with_span(
            MSG_KIND_ERROR,
            "unexpected end of file",
            current(p)->span);
        msg_addl_fat(&msg, aria_format("while matching `%to`", pair), pair->span);
        msg_emit(p, &msg);
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
    if (current(p)->kind == TOKEN_KIND_KEYWORD &&
        is_token_lexeme(current(p), keyword)) {
        goto_next_tok(p);
        return true;
    }
    return false;
}

static Token* expect(ParseCtx* p, TokenKind kind, const char* expected) {
    if (!match(p, kind)) {
        Msg msg = msg_with_span(
            MSG_KIND_ERROR,
            aria_format("expected %s", expected),
            current(p)->span);
        msg_emit(p, &msg);
        return NULL;
    }
    return previous(p);
}

static Token* expect_keyword(ParseCtx* p, const char* keyword) {
    return expect(p, TOKEN_KIND_KEYWORD, aria_format("keyword `%s`, keyword"));
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

static TypePrimitiveKind get_kindof_prim_type(Token* tok) {
    if (tok->kind != TOKEN_KIND_IDENTIFIER) return TYPE_PRIM_NONE;
    
    if (is_token_lexeme(tok, "u8")) return TYPE_PRIM_U8;
    else if (is_token_lexeme(tok, "u16")) return TYPE_PRIM_U16;
    else if (is_token_lexeme(tok, "u32")) return TYPE_PRIM_U32;
    else if (is_token_lexeme(tok, "u64")) return TYPE_PRIM_U64;
    else if (is_token_lexeme(tok, "i8")) return TYPE_PRIM_I8;
    else if (is_token_lexeme(tok, "i16")) return TYPE_PRIM_I16;
    else if (is_token_lexeme(tok, "i32")) return TYPE_PRIM_I32;
    else if (is_token_lexeme(tok, "i64")) return TYPE_PRIM_I64;
    else if (is_token_lexeme(tok, "void")) return TYPE_PRIM_VOID;
    return TYPE_PRIM_NONE;
}

static AstNode* parse_type(ParseCtx* p) {
    Token* tok = current(p);
    if (tok->kind == TOKEN_KIND_IDENTIFIER) {
        goto_next_tok(p);
        Type type;
        TypePrimitiveKind kind = get_kindof_prim_type(tok);
        if (kind != TYPE_PRIM_NONE) {
            type = type_primitive_init(kind);
        } else {
            type = type_custom_init(tok);
        }
        return astnode_type_new(type, tok->span);
    } 

    Msg msg = msg_with_span(
        MSG_KIND_ERROR,
        "expected type",
        tok->span);
    msg_emit(p, &msg);
    return NULL;
}

static AstNode* parse_atom_expr(ParseCtx* p, bool error_on_no_match) {
    if (match(p, TOKEN_KIND_IDENTIFIER)) {
        return astnode_symbol_new(previous(p));
    }

    if (error_on_no_match) {
    	Msg msg = msg_with_span(
    	    MSG_KIND_ERROR,
    	    "expected expression",
    	    current(p)->span);
    	msg_emit(p, &msg);
    }
    return NULL;
}

static AstNode* parse_expr(ParseCtx* p, bool error_on_no_match) {
    return parse_atom_expr(p, error_on_no_match);
}

static AstNode* parse_function_header(ParseCtx* p, Token* keyword) {
    Token* identifier = expect_identifier(p, "function name");
    Token* lparen = expect_lparen(p);

    AstNode** params = NULL;
    while (!match(p, TOKEN_KIND_RPAREN)) {
        check_eof(p, lparen);
        Token* param_identifier =
            expect(p, TOKEN_KIND_IDENTIFIER, "parameter name or `)`");
        expect_colon(p);
        AstNode* param_type = parse_type(p);
        bufpush(params, astnode_param_new(param_identifier, param_type));
        if (current(p)->kind != TOKEN_KIND_RPAREN) {
            expect(p, TOKEN_KIND_COMMA, "`,' or ')'");
        }
    }

    AstNode* ret_type = parse_type(p);
    return astnode_function_header_new(keyword, identifier, params, ret_type);
}

static AstNode* parse_scoped_block(ParseCtx* p, Token* lbrace) {
    // TODO: parse val
    AstNode** stmts = NULL;
    while (current(p)->kind != TOKEN_KIND_RBRACE) {
        check_eof(p, lbrace);
        AstNode* stmt = parse_root(p, false);
        if (stmt) bufpush(stmts, stmt);
        else {
            AstNode* expr = parse_expr(p, false);
            if (!expr) {
                Msg msg = msg_with_span(
                    MSG_KIND_ERROR,
                    "expected a declaration, definition or an expression",
                    current(p)->span);
                msg_emit(p, &msg);
            }
            expect_semicolon(p);
            // We directly push an AstNode of type expr_stmt
            bufpush(stmts, expr);
        }
    }
    goto_next_tok(p);
    return astnode_scoped_block_new(lbrace, stmts, NULL, previous(p));
}

static AstNode* parse_root(ParseCtx* p, bool error_on_no_match) {
    if (match_keyword(p, "fn")) {
        AstNode* header = parse_function_header(p, previous(p));
        Token* lbrace = expect_lbrace(p);
        AstNode* body = parse_scoped_block(p, lbrace);
        return astnode_function_def_new(header, body);
    }

    if (error_on_no_match) {
    	Msg msg = msg_with_span(
    	    MSG_KIND_ERROR,
    	    "expected a declaration or a definition",
    	    current(p)->span);
    	msg_emit(p, &msg);
    }
    return NULL;
}

void parse(ParseCtx* p) {
    while (current(p)->kind != TOKEN_KIND_EOF) {
        AstNode* astnode = parse_root(p, true);
        if (astnode) bufpush(p->srcfile->astnodes, astnode);
    }
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
