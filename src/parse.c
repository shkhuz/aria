#include "parse.h"
#include "printf.h"
#include "buf.h"
#include "msg.h"

static AstNode* parse_root(ParseCtx* p, bool error_on_no_match);
static AstNode* parse_expr(ParseCtx* p, const char* custom_msg);
static AstNode* parse_scoped_block(ParseCtx* p, Token* lbrace);
static AstNode* parse_typespec(ParseCtx* p);

ParseCtx parse_new_context(Srcfile* srcfile) {
    ParseCtx p;
    p.srcfile = srcfile;
    p.srcfile->astnodes = NULL;
    p.token_idx = 0;
    p.current = p.srcfile->tokens[0];
    p.prev = NULL;
    p.error = false;
    return p;
}

static inline void msg_emit(ParseCtx* p, Msg* msg) {
    if (msg->kind == MSG_ERROR) p->error = true;
    _msg_emit(msg);
    terminate_compilation();
}

static void goto_next_tok(ParseCtx* p) {
    if (p->token_idx < buflen(p->srcfile->tokens)) {
        p->token_idx++;
        p->prev = p->current;
        p->current = p->srcfile->tokens[p->token_idx];
    }
}

//static void goto_prev_tok(ParseCtx* p) {
//    if (p->token_idx > 0) {
//        p->token_idx--;
//    }
//}

static void check_eof(ParseCtx* p, Token* pair) {
    if (p->current->kind == TOKEN_EOF) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "unexpected end of file",
            p->current->span);
        msg_addl_fat(&msg, "while matching...", pair->span);
        msg_emit(p, &msg);
    }
}

static bool match(ParseCtx* p, TokenKind kind) {
    if (p->current->kind == kind) {
        goto_next_tok(p);
        return true;
    }
    return false;
}

static Token* expect(ParseCtx* p, TokenKind kind, const char* expected) {
    if (!match(p, kind)) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            aria_format("expected %s", expected),
            p->current->span);
        msg_emit(p, &msg);
        return NULL;
    }
    return p->prev;
}

static Token* expect_identifier(ParseCtx* p, const char* expected) {
    return expect(p, TOKEN_IDENTIFIER, expected);
}

static Token* expect_lparen(ParseCtx* p) {
    return expect(p, TOKEN_LPAREN, "`(`");
}

static Token* expect_rparen(ParseCtx* p) {
    return expect(p, TOKEN_RPAREN, "`)`");
}

static Token* expect_lbrace(ParseCtx* p) {
    return expect(p, TOKEN_LBRACE, "`{`");
}

static Token* expect_colon(ParseCtx* p) {
    return expect(p, TOKEN_COLON, "`:`");
}

static Token* expect_semicolon(ParseCtx* p) {
    return expect(p, TOKEN_SEMICOLON, "`;`");
}

static Token* expect_comma(ParseCtx* p) {
    return expect(p, TOKEN_COMMA, "`,`");
}

static AstNode* parse_symbol(ParseCtx* p, bool is_type) {
    Token* first = p->prev;
    assert(first->kind == TOKEN_IDENTIFIER);
    return astnode_symbol_new(first, is_type);
}

static inline AstNode* parse_typespec_symbol(ParseCtx* p) {
    return parse_symbol(p, true);
}

static AstNode* parse_typespec_ptr(ParseCtx* p) {
    Token* star = p->prev;
    AstNode* child = parse_typespec(p);
    return astnode_typespec_ptr_new(star, child);
}

static AstNode* parse_typespec(ParseCtx* p) {
    if (match(p, TOKEN_STAR)) {
        return parse_typespec_ptr(p);
    } else if (match(p, TOKEN_IDENTIFIER)) {
        return parse_typespec_symbol(p);
    }

    Msg msg = msg_with_span(
        MSG_ERROR,
        "expected type",
        p->current->span);
    msg_emit(p, &msg);
    return NULL;
}

static AstNode* parse_if_branch(ParseCtx* p, Token* keyword, bool elsebr) {
    AstNode* cond = NULL;
    if (!elsebr) {
        expect_lparen(p);
        cond = parse_expr(p, NULL);
        expect_rparen(p);
    }
    
    AstNode* body = parse_expr(p, NULL);
    if (body->kind == ASTNODE_IF) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "`if` body may not contain another `if`",
            body->span);
        msg_addl_thin(&msg, "enclose `if` body with `{}`");
        msg_emit(p, &msg);
    }
    return astnode_if_branch_new(keyword, cond, body);
}

static AstNode* get_last_if_branch(AstNode* ifbr, AstNode** elseifbr, AstNode* elsebr) {
    if (elsebr) return elsebr;
    else if (elseifbr) return elseifbr[buflen(elseifbr)-1];
    return ifbr;
}

static AstNode* parse_atom_expr(ParseCtx* p, const char* custom_msg) {
    if (match(p, TOKEN_IDENTIFIER)) {
        return parse_symbol(p, false);
    } else if (match(p, TOKEN_LBRACE)) {
        return parse_scoped_block(p, p->prev);
    } else if (match(p, TOKEN_KEYWORD_IF)) {
        AstNode* ifbr = parse_if_branch(p, p->prev, false);
        AstNode** elseifbr = NULL;
        AstNode* elsebr = NULL;
        
        for (;;) {
            if (match(p, TOKEN_KEYWORD_ELSE)) {
                Token* keyword = p->prev;
                if (match(p, TOKEN_KEYWORD_IF)) {
                    bufpush(elseifbr, parse_if_branch(p, keyword, false));
                } else {
                    elsebr = parse_if_branch(p, keyword, true);
                    break;
                }
            }
            else break;
        }
        
        return astnode_if_new(
            ifbr,
            elseifbr,
            elsebr,
            get_last_if_branch(ifbr, elseifbr, elsebr));
    } else if (match(p, TOKEN_KEYWORD_RETURN)) {
        Token* keyword = p->prev;
        AstNode* operand = NULL;
        if (can_token_start_expr(p->current)) {
            operand = parse_expr(p, NULL);
        }
        return astnode_return_new(keyword, operand);
    } else if (match(p, TOKEN_INTEGER_LITERAL)) {
        Token* token = p->prev;
        bigint val;
        bigint_init(&val);
        // TODO: move so that these are only initialized once
        bigint base;
        bigint_init_u64(&base, 10);
        bigint digit;
        bigint_init(&digit);
        
        for (usize i = token->span.start; i < token->span.end; i++) {
            char c = token->span.srcfile->handle.contents[i];
            if (c != '_') {
            	int d = c - '0';
                bigint_set_u64(&digit, (u64)d);
            	bigint_mul(&val, &base, &val);
            	bigint_add(&val, &digit, &val);
            }
        }
        
        return astnode_integer_literal_new(token, val);
    }
    // NOTE: Add the case to `can_token_begin_expr()`

    Msg msg = msg_with_span(
        MSG_ERROR,
        custom_msg ? custom_msg : "expected expression",
        p->current->span);
    msg_emit(p, &msg);
    return NULL;
}

static AstNode* parse_suffix_expr(ParseCtx* p, const char* custom_msg) {
    AstNode* left = parse_atom_expr(p, custom_msg);
    if (match(p, TOKEN_LPAREN)) {
        Token* lparen = p->prev;
        AstNode** args = NULL;
        while (!match(p, TOKEN_RPAREN)) {
            check_eof(p, lparen);
            AstNode* arg = parse_expr(p, NULL);
            bufpush(args, arg);
            if (p->current->kind != TOKEN_RPAREN) {
                expect(p, TOKEN_COMMA, "`,' or ')'");
            }
        }
        return astnode_function_call_new(left, args, p->prev);
    }
    return left;
}

// Note: `custom_msg` may be passed null
static AstNode* parse_expr(ParseCtx* p, const char* custom_msg) {
    return parse_suffix_expr(p, custom_msg);
}

static AstNode* parse_function_header(ParseCtx* p, Token* keyword) {
    Token* identifier = expect_identifier(p, "function name");
    Token* lparen = expect_lparen(p);

    AstNode** params = NULL;
    while (!match(p, TOKEN_RPAREN)) {
        check_eof(p, lparen);
        Token* param_identifier =
            expect(p, TOKEN_IDENTIFIER, "parameter name or `)`");
        expect_colon(p);
        AstNode* param_type = parse_typespec(p);
        bufpush(params, astnode_param_new(param_identifier, param_type));
        if (p->current->kind != TOKEN_RPAREN) {
            expect(p, TOKEN_COMMA, "`,' or ')'");
        }
    }

    AstNode* ret_type = parse_typespec(p);
    return astnode_function_header_new(keyword, identifier, params, ret_type);
}

static AstNode* parse_scoped_block(ParseCtx* p, Token* lbrace) {
    // TODO: parse val
    AstNode** stmts = NULL;
    AstNode* val = NULL;
    while (!match(p,TOKEN_RBRACE)) {
        check_eof(p, lbrace);
        AstNode* stmt = parse_root(p, false);
        if (stmt) bufpush(stmts, stmt);
        else {
            // Custom error message for parsing an expression inside a
            // scoped block
            AstNode* expr = parse_expr(
                p,
                "expected a declaration, definition or an expression");
            if (p->current->kind == TOKEN_RBRACE) val = expr;
            else {
                if ((expr->kind == ASTNODE_IF 
                     && get_last_if_branch(
                            expr->iff.ifbr, 
                            expr->iff.elseifbr, 
                            expr->iff.elsebr)->ifbr.body->kind == ASTNODE_SCOPED_BLOCK)) {
                } else {
                    expect_semicolon(p);
                }
            	// We directly push an AstNode of type expr_stmt
            	bufpush(stmts, expr);
            }
        }
    }
    return astnode_scoped_block_new(lbrace, stmts, val, p->prev);
}

static AstNode* parse_root(ParseCtx* p, bool error_on_no_match) {
    if (match(p, TOKEN_KEYWORD_FN)) {
        AstNode* header = parse_function_header(p, p->prev);
        Token* lbrace = expect_lbrace(p);
        AstNode* body = parse_scoped_block(p, lbrace);
        return astnode_function_def_new(header, body);
    } else if (match(p, TOKEN_KEYWORD_IMM) || match(p, TOKEN_KEYWORD_MUT)) {
        Token* keyword = p->prev;
        bool immutable = true;
        if (keyword->kind == TOKEN_KEYWORD_MUT) immutable = false; 
        Token* identifier = expect_identifier(p, "variable name");
        AstNode* type = NULL;
        AstNode* initializer = NULL;
        if (match(p, TOKEN_COLON)) {
            type = parse_typespec(p);
        }
        if (match(p, TOKEN_EQUAL)) {
            initializer = parse_expr(p, NULL);
        }
        expect_semicolon(p);
        return astnode_variable_decl_new(
            keyword,
            immutable,
            identifier,
            type,
            initializer,
            p->prev);
    }

    if (error_on_no_match) {
    	Msg msg = msg_with_span(
    	    MSG_ERROR,
    	    "expected a declaration or a definition",
    	    p->current->span);
    	msg_emit(p, &msg);
    }
    return NULL;
}

void parse(ParseCtx* p) {
    while (p->current->kind != TOKEN_EOF) {
        AstNode* astnode = parse_root(p, true);
        if (astnode) bufpush(p->srcfile->astnodes, astnode);
    }
}
