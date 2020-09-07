#include <parser.h>
#include <ast_print.h>
#include <token.h>
#include <expr.h>
#include <error_msg.h>
#include <arpch.h>

static bool match(Parser* self, TokenType type);
static Token* current(Parser* self);
static void goto_next_token(Parser* self);

Parser parser_new(File* srcfile, Token** tokens) {
    Parser parser;
    parser.srcfile = srcfile;
    parser.tokens = tokens;
    parser.tokens_idx = 0;
    parser.stmts = null;
    parser.error_panic = false;
    parser.error_count = 0;
    parser.error_loc = ERRLOC_GLOBAL;
    return parser;
}

#define check_eof \
    if (current(self)->type == T_EOF) return

static void sync_to_next_statement(Parser* self) {
    switch (self->error_loc) {
    case ERRLOC_GLOBAL:
    case ERRLOC_FUNCTION_DEF_BODY:
        while (!match(self, T_SEMICOLON) && current(self)->type != T_L_BRACE) {
            check_eof;
            goto_next_token(self);
        }
        if (current(self)->type == T_L_BRACE) goto skip_brace;
        break;

    case ERRLOC_FUNCTION_DEF: {
    skip_brace: {
            u64 brace_count = 0;
            while (!match(self, T_L_BRACE)) {
                check_eof;
                goto_next_token(self);
            }
            brace_count++;

            while (brace_count != 0) {
                check_eof;
                if (match(self, T_L_BRACE)) {
                    brace_count++;
                    continue;
                }

                check_eof;
                else if (match(self, T_R_BRACE)) {
                    brace_count--;
                    continue;
                }

                check_eof; goto_next_token(self);
            }
        }} break;
    }
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
            ap
    );
    sync_to_next_statement(self);
    va_end(ap);
}

#define es \
    u64 COMBINE(_error_store,__LINE__) = self->error_count

#define er \
    if (self->error_count > COMBINE(_error_store,__LINE__)) return

#define ern \
    er null

#define chk(stmt) \
    es; stmt; ern;

#define EXPR_CI(name, init_func, ...) \
    Expr* name = null; \
    { \
        es; \
        name = init_func(__VA_ARGS__); \
        ern; \
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

static bool match_keyword(Parser* self, const char* keyword) {
    if (current(self)->type == T_KEYWORD &&
        str_intern(current(self)->lexeme) == str_intern(keyword)) {
        goto_next_token(self);
        return true;
    }
    return false;
}

static void expect(Parser* self, TokenType type, const char* fmt, ...) {
    if (!match(self, type)) {
        va_list ap;
        va_start(ap, fmt);
        error_token_with_sync(self, current(self), fmt, ap);
        va_end(ap);
    }
}

static void expect_keyword(Parser* self, const char* keyword) {
    if (!match_keyword(self, keyword)) {
        error_token_with_sync(
                self,
                current(self),
                "expect keyword: `%s`",
                keyword
        );
    }
}

#define expect_identifier(self) \
    chk(expect(self, T_IDENTIFIER, "expect identifier"))

#define expect_semicolon(self) \
    chk(expect(self, T_SEMICOLON, "expect ';'"))

#define expect_double_colon(self) \
    chk(expect(self, T_DOUBLE_COLON, "expect '::'"))

#define expect_l_brace(self) \
    chk(expect(self, T_L_BRACE, "expect '{'"))

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
                current(self)->lexeme
        );
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
    expect_semicolon(self);
    return stmt_expr_new(e);
}

static Stmt* stmt_function_def_new(Token* identifier, Stmt** body) {
    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_FUNCTION_DEF;
    stmt->s.function_def.identifier = identifier;
    stmt->s.function_def.body = body;
    return stmt;
}

static Stmt* decl(Parser* self) {
    self->error_loc = ERRLOC_GLOBAL;
    if (match_keyword(self, "fn")) {
        self->error_loc = ERRLOC_FUNCTION_DEF;
        expect_identifier(self);
        Token* identifier = previous(self);

        expect_l_brace(self);

        Stmt** body = null;
        self->error_loc = ERRLOC_FUNCTION_DEF_BODY;
        while (!match(self, T_R_BRACE)) {
            Stmt* s = stmt(self);
            if (s) buf_push(body, s);
        }

        return stmt_function_def_new(identifier, body);
    }
    else {
        error_token_with_sync(
                self,
                current(self),
                "expect top-level declaration"
        );
        return null;
    }
}

void parser_run(Parser* self) {
    while (current(self)->type != T_EOF) {
        Stmt* s = decl(self);
        if (s) buf_push(self->stmts, s);
    }
}

