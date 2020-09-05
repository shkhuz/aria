#include <parser.h>
#include <ast_print.h>
#include <token.h>
#include <expr.h>
#include <arpch.h>

Parser parser_new(File* srcfile, Token** tokens) {
    Parser parser;
    parser.srcfile = srcfile;
    parser.tokens = tokens;
    parser.tokens_idx = 0;
    parser.stmts = null;
    return parser;
}

static Token* current(Parser* p) {
    return p->tokens[p->tokens_idx];
}
#define current() current(p)

static Token* previous(Parser* p) {
    if (p->tokens_idx > 0) {
        return p->tokens[p->tokens_idx - 1];
    }
    else {
        assert(0);
    }
}
#define previous() previous(p)

static void goto_next_token(Parser* p) {
    if (p->tokens_idx < buf_len(p->tokens)) {
        p->tokens_idx++;
    }
}

static bool match(Parser* p, TokenType type) {
    if (current()->type == type) {
        goto_next_token(p);
        return true;
    }
    return false;
}
#define match(type) match(p, type)

static Expr* expr_integer_new(Token* integer) {
    Expr* expr = expr_new_alloc();
    expr->type = E_INTEGER;
    expr->e.integer = integer;
    return expr;
}

static Expr* expr_unary_new(Token* op, Expr* right) {
    Expr* expr = expr_new_alloc();
    expr->type = E_UNARY;
    expr->e.unary.op = op;
    expr->e.unary.right = right;
    return expr;
}

static Expr* expr_binary_new(Expr* left, Expr* right, Token* op) {
    Expr* expr = expr_new_alloc();
    expr->type = E_BINARY;
    expr->e.binary.left = left;
    expr->e.binary.right = right;
    expr->e.binary.op = op;
    return expr;
}

static Expr* expr_atom(Parser* p) {
    if (match(T_INTEGER)) {
        return expr_integer_new(previous());
    }
    else {
        assert(0);
    }
}

static Expr* expr_unary_add_sub(Parser* p) {
    if (match(T_PLUS) || match(T_MINUS)) {
        Token* op = previous();
        Expr* right = expr_unary_add_sub(p);
        return expr_unary_new(op, right);
    }
    return expr_atom(p);
}

static Expr* expr_binary_mul_div(Parser* p) {
    Expr* left = expr_unary_add_sub(p);
    while (match(T_STAR) || match(T_FW_SLASH)) {
        Token* op = previous();
        Expr* right = expr_unary_add_sub(p);
        left = expr_binary_new(left, right, op);
    }
    return left;
}

static Expr* expr_binary_add_sub(Parser* p) {
    Expr* left = expr_binary_mul_div(p);
    while (match(T_PLUS) || match(T_MINUS)) {
        Token* op = previous();
        Expr* right = expr_binary_mul_div(p);
        left = expr_binary_new(left, right, op);
    }
    return left;
}

static Expr* expr(Parser* p) {
    return expr_binary_add_sub(p);
}

void parser_run(Parser* p) {
    print_expr(expr(p));
}
