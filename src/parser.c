#include <parser.h>
#include <ast_print.h>
#include <token.h>
#include <expr.h>
#include <error_msg.h>
#include <arpch.h>

static bool match(Parser* self, TokenType type);
static void goto_next_token(Parser* self);

Parser parser_new(File* srcfile, Token** tokens) {
    Parser parser;
    parser.srcfile = srcfile;
    parser.tokens = tokens;
    parser.tokens_idx = 0;
    parser.stmts = null;
    parser.error_panic = false;
    parser.error_count = 0;
    return parser;
}

static void sync_to_next_statement(Parser* self) {
    while (!match(self, T_SEMICOLON)) {
        goto_next_token(self);
    }
    return;
}

static void error_token_with_sync(
        Parser* self,
        Token* token,
        const char* fmt,
        ...) {

    self->error_count++;
    va_list ap;
    va_start(ap, fmt);
    verror_token(
            self->srcfile,
            token,
            fmt,
            ap);
    sync_to_next_statement(self);
    va_end(ap);
}

#define error_store \
    u64 _error_store = self->error_count;

#define error_return \
    if (self->error_count > _error_store) return

#define EXPR_CI(name, init_func, ...) \
    Expr* name = null; \
    { \
        error_store; \
        name = init_func(__VA_ARGS__); \
        error_return null; \
    }

#define EXPR(name) \
    EXPR_CI(name, expr, self)

static Token* current(Parser* self) {
    return self->tokens[self->tokens_idx];
}

static Token* previous(Parser* self) {
    if (self->tokens_idx > 0) {
        return self->tokens[self->tokens_idx - 1];
    }
    else {
        assert(0);
    }
}

static void goto_next_token(Parser* self) {
    if (self->tokens_idx < buf_len(self->tokens)) {
        self->tokens_idx++;
    }
}

static bool match(Parser* self, TokenType type) {
    if (current(self)->type == type) {
        goto_next_token(self);
        return true;
    }
    return false;
}

static void consume(Parser* self, TokenType type, const char* fmt, ...) {
    if (!match(self, type)) {
        va_list ap;
        va_start(ap, fmt);
        error_token_with_sync(self, current(self), fmt, ap);
        va_end(ap);
    }
}

#define consume_semicolon(self) \
    consume(self, T_SEMICOLON, "expect ';'")

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

static Expr* expr_atom(Parser* self) {
    if (match(self, T_INTEGER)) {
        return expr_integer_new(previous(self));
    }
    else {
        error_token_with_sync(
                self,
                current(self),
                "invalid ast: `%s` is not expected here",
                current(self)->lexeme);
        return null;
    }
}

static Expr* expr_unary_add_sub(Parser* self) {
    if (match(self, T_PLUS) || match(self, T_MINUS)) {
        Token* op = previous(self);
        EXPR_CI(right, expr_unary_add_sub, self);
        return expr_unary_new(op, right);
    }
    return expr_atom(self);
}

static Expr* expr_binary_mul_div(Parser* self) {
    EXPR_CI(left, expr_unary_add_sub, self);
    while (match(self, T_STAR) || match(self, T_FW_SLASH)) {
        Token* op = previous(self);
        EXPR_CI(right, expr_unary_add_sub, self);
        left = expr_binary_new(left, right, op);
    }
    return left;
}

static Expr* expr_binary_add_sub(Parser* self) {
    EXPR_CI(left, expr_binary_mul_div, self);
    while (match(self, T_PLUS) || match(self, T_MINUS)) {
        Token* op = previous(self);
        EXPR_CI(right, expr_binary_mul_div, self);
        left = expr_binary_new(left, right, op);
    }
    return left;
}

static Expr* expr(Parser* self) {
    return expr_binary_add_sub(self);
}

static Stmt* stmt_expr_new(Expr* expr) {
    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_EXPR;
    stmt->s.expr = expr;
    return stmt;
}

static Stmt* stmt(Parser* self) {
    EXPR(e);
    consume_semicolon(self);
    return stmt_expr_new(e);
}

void parser_run(Parser* self) {
    while (current(self)->type != T_EOF) {
        Stmt* s = stmt(self);
        buf_push(self->stmts, s);
    }
}

