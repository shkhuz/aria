#include "parse.h"
#include "buf.h"
#include "stri.h"
#include "msg.h"

typedef struct {
    bool is_stmt;
    union {
        Stmt* stmt;
        Expr* expr;
    };
} StmtOrExpr;

static Stmt* parse_top_level_stmt(ParseContext* p, bool error_on_no_match);
static FunctionHeader* parse_function_header(ParseContext* p);
static Stmt* parse_variable_stmt(ParseContext* p);
static StmtOrExpr parse_function_level_node(ParseContext* p);
static Stmt* parse_expr_stmt(ParseContext* p, Expr* expr);
static Expr* parse_expr(ParseContext* p);
static Expr* parse_atom_expr(ParseContext* p);
static Expr* parse_integer_expr(ParseContext* p);
static Expr* parse_symbol_expr(ParseContext* p, Token* identifier);
static Expr* parse_function_call_expr(
        ParseContext* p, 
        Expr* callee, 
        Token* lparen);
static Expr* parse_block_expr(ParseContext* p, Token* lbrace);
static Type* parse_type(ParseContext* p);
static Type* parse_ptr_type(ParseContext* p);
static Type* parse_atom_type(ParseContext* p);
static Token* parse_current(ParseContext* p);
static Token* parse_next(ParseContext* p);
static Token* parse_previous(ParseContext* p);
static void parse_goto_next_token(ParseContext* p);
static void parse_goto_previous_token(ParseContext* p);
static void parse_check_eof(ParseContext* p, Token* pair);
static bool parse_match(ParseContext* p, TokenKind kind);
static bool parse_match_keyword(ParseContext* p, char* keyword);
static Token* parse_expect_keyword(ParseContext* p, char* keyword);
static Token* parse_expect(ParseContext* p, TokenKind kind, char* expect);
static Token* parse_expect_identifier(ParseContext* p, char* expect);
static Token* parse_expect_lbrace(ParseContext* p);
static Token* parse_expect_rbrace(ParseContext* p);
static Token* parse_expect_lparen(ParseContext* p);
static Token* parse_expect_rparen(ParseContext* p);
static Token* parse_expect_colon(ParseContext* p);
static Token* parse_expect_semicolon(ParseContext* p);
static Token* parse_expect_comma(ParseContext* p);

void parse(ParseContext* p) {
    p->srcfile->stmts = null;
    p->token_idx = 0;
    p->error = false;

    while (parse_current(p)->kind != TOKEN_KIND_EOF) {
        Stmt* stmt = parse_top_level_stmt(p, true);
        if (stmt) buf_push(p->srcfile->stmts, stmt);
    }
}

Stmt* parse_top_level_stmt(ParseContext* p, bool error_on_no_match) {
    if (parse_match_keyword(p, "fn")) {
        FunctionHeader* header = parse_function_header(p);
        Token* lbrace = parse_expect_lbrace(p);
        Expr* body = parse_block_expr(p, lbrace);
        return function_stmt_new(header, body);
    }
    else if (parse_match_keyword(p, "let")) {
        return parse_variable_stmt(p);
    }
    else if (error_on_no_match) {
        fatal_error(
                parse_current(p),
                "invalid token in top-level");
    }
    return null;
}

FunctionHeader* parse_function_header(ParseContext* p) {
    Token* identifier = parse_expect_identifier(p, "function name");
    Token* lparen = parse_expect_lparen(p);

    Stmt** params = null;
    size_t param_idx = 0;
    while (!parse_match(p, TOKEN_KIND_RPAREN)) {
        parse_check_eof(p, lparen);
        Token* param_identifier = parse_expect_identifier(p, "parameter");
        parse_expect_colon(p);
        Type* param_type = parse_type(p);
        buf_push(
                params, 
                param_stmt_new(param_identifier, param_type, param_idx));

        if (parse_current(p)->kind != TOKEN_KIND_RPAREN) {
            parse_expect_comma(p);
        }
        param_idx++;
    }

    Type* return_type = builtin_type_placeholders.void_type;
    if (parse_current(p)->kind != TOKEN_KIND_LBRACE) {
        return_type = parse_type(p);
    }

    return function_header_new(
            identifier,
            params,
            return_type);
}

Stmt* parse_variable_stmt(ParseContext* p) {
    bool constant = true;
    if (parse_match_keyword(p, "mut")) {
        constant = false;
    }

    Token* identifier = parse_expect_identifier(p, "variable name");
    Type* type = null;
    if (parse_match(p, TOKEN_KIND_COLON)) {
        type = parse_type(p);
    }

    Expr* initializer = null;
    if (parse_match(p, TOKEN_KIND_EQUAL)) {
        initializer = parse_expr(p);
    }

    parse_expect_semicolon(p);
    return variable_stmt_new(
            constant,
            identifier,
            type,
            initializer);
}

StmtOrExpr parse_function_level_node(ParseContext* p) {
    StmtOrExpr result;
    result.is_stmt = true;
    result.stmt = null;

    if (parse_match_keyword(p, "let")) {
        result.stmt = parse_variable_stmt(p);
        return result;
    }

    Expr* expr = parse_expr(p);
    if (parse_current(p)->kind == TOKEN_KIND_RBRACE) {
        result.is_stmt = false;
        result.expr = expr;
        return result;
    }
    result.stmt = parse_expr_stmt(p, expr);
    return result;
}

Stmt* parse_expr_stmt(ParseContext* p, Expr* expr) {
    assert(expr);
    if (expr->kind != EXPR_KIND_BLOCK) {
        parse_expect_semicolon(p);
    }
    return expr_stmt_new(expr);
}

Expr* parse_expr(ParseContext* p) {
    return parse_atom_expr(p);
}

Expr* parse_atom_expr(ParseContext* p) {
    if (parse_match(p, TOKEN_KIND_INTEGER)) {
        return parse_integer_expr(p);
    }
    else if (parse_match(p, TOKEN_KIND_IDENTIFIER)) {
        Expr* symbol = parse_symbol_expr(p, parse_previous(p));
        if (parse_match(p, TOKEN_KIND_LPAREN)) {
            return parse_function_call_expr(p, symbol, parse_previous(p));
        }
        else {
            return symbol;
        }
    }
    else if (parse_match(p, TOKEN_KIND_LBRACE)) {
        return parse_block_expr(p, parse_previous(p));
    }
    else {
        fatal_error(
                parse_current(p),
                "`{s}` is invalid here",
                parse_current(p)->lexeme);
    }
    return null;
}

Expr* parse_integer_expr(ParseContext* p) {
    Token* integer = parse_previous(p);
    return integer_expr_new(integer, integer->integer.val);
}

Expr* parse_symbol_expr(ParseContext* p, Token* identifier) {
    return symbol_expr_new(identifier);
}

Expr* parse_function_call_expr(
        ParseContext* p, 
        Expr* callee, 
        Token* lparen) {
    
    Expr** args = null;
    while (!parse_match(p, TOKEN_KIND_RPAREN)) {
        parse_check_eof(p, lparen);
        Expr* arg = parse_expr(p);
        buf_push(args, arg);
        if (parse_current(p)->kind != TOKEN_KIND_RPAREN) {
            parse_expect_comma(p);
        }
    }
    return function_call_expr_new(callee, args, parse_previous(p));
}

Expr* parse_block_expr(ParseContext* p, Token* lbrace) {
    Stmt** stmts = null;
    Expr* value = null;
    while (!parse_match(p, TOKEN_KIND_RBRACE)) {
        parse_check_eof(p, lbrace);
        StmtOrExpr meta = parse_function_level_node(p);
        if (meta.is_stmt) {
            buf_push(stmts, meta.stmt);
        }
        else {
            value = meta.expr;
        }
    }
    return block_expr_new(lbrace, stmts, value);
}

Type* parse_type(ParseContext* p) {
    return parse_ptr_type(p);
}

Type* parse_ptr_type(ParseContext* p) {
    if (parse_match(p, TOKEN_KIND_STAR)) {
        Token* star = parse_previous(p);
        bool constant = false;
        if (parse_match_keyword(p, "const")) {
            constant = true;
        }
        Type* child = parse_ptr_type(p);
        return ptr_type_new(
                star,
                constant,
                child);
    }
    return parse_atom_type(p);
}

Type* parse_atom_type(ParseContext* p) {
    Token* identifier = parse_expect_identifier(p, "type name");
    BuiltinTypeKind builtin_type_kind = 
        builtin_type_str_to_kind(identifier->lexeme);
    if (builtin_type_kind == BUILTIN_TYPE_KIND_NONE) {
        // TODO: implement custom types
        assert(0);
    }
    return builtin_type_new(identifier, builtin_type_kind);
}

Token* parse_current(ParseContext* p) {
    if (p->token_idx < buf_len(p->srcfile->tokens)) {
        return p->srcfile->tokens[p->token_idx];
    }
    assert(0);
    return null;
}

Token* parse_next(ParseContext* p) {
    if ((p->token_idx+1) < buf_len(p->srcfile->tokens)) {
        return p->srcfile->tokens[p->token_idx+1];
    }
    assert(0);
    return null;
}

Token* parse_previous(ParseContext* p) {
    if (p->token_idx > 0) {
        return p->srcfile->tokens[p->token_idx-1];
    }
    assert(0);
    return null;
}

void parse_goto_next_token(ParseContext* p) {
    if (p->token_idx < buf_len(p->srcfile->tokens)) {
        p->token_idx++;
    }
}

void parse_goto_previous_token(ParseContext* p) {
    if (p->token_idx > 0) {
        p->token_idx--;
    }
}

void parse_check_eof(ParseContext* p, Token* pair) {
    if (parse_current(p)->kind == TOKEN_KIND_EOF) {
        note(
                pair,
                "while matching `{s}`...",
                pair->lexeme);
        fatal_error(parse_current(p), "unexpected end of file");
    }
}

bool parse_match(ParseContext* p, TokenKind kind) {
    if (parse_current(p)->kind == kind) {
        parse_goto_next_token(p);
        return true;
    }
    return false;
}

bool parse_match_keyword(ParseContext* p, char* keyword) {
    if (parse_current(p)->kind == TOKEN_KIND_KEYWORD && 
        stri(parse_current(p)->lexeme) == stri(keyword)) {
        parse_goto_next_token(p);
        return true;
    }
    return false;
}

Token* parse_expect_keyword(ParseContext* p, char* keyword) {
    if (parse_match_keyword(p, keyword)) {
        return parse_previous(p);
    }

    fatal_error(
            parse_current(p), 
            "expected keyword `{s}`, got `{s}`",
            keyword,
            parse_current(p)->lexeme);
    return null;
}

Token* parse_expect(ParseContext* p, TokenKind kind, char* expect) {
    if (parse_match(p, kind)) {
        return parse_previous(p);
    }

    fatal_error(
            parse_current(p),
            "expected {s}, got `{s}`",
            expect,
            parse_current(p)->lexeme);
    return null;
}

Token* parse_expect_identifier(ParseContext* p, char* expect) {
    return parse_expect(p, TOKEN_KIND_IDENTIFIER, expect);
}

Token* parse_expect_lbrace(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_LBRACE,
            "`{`");
}

Token* parse_expect_rbrace(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_RBRACE,
            "`}`");
}

Token* parse_expect_lparen(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_LPAREN,
            "`(`");
}

Token* parse_expect_rparen(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_RPAREN,
            "`)`");
}

Token* parse_expect_colon(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_COLON,
            "`:`");
}

Token* parse_expect_semicolon(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_SEMICOLON,
            "`;`");
}

Token* parse_expect_comma(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_COMMA,
            "`,`");
}
