#include <parser.h>
#include <ast_print.h>
#include <token.h>
#include <data_type.h>
#include <expr.h>
#include <error_msg.h>
#include <arpch.h>

static bool match(Parser* self, TokenType type);
static Token* current(Parser* self);
static void goto_next_token(Parser* self);
static void goto_previous_token(Parser* self);

Parser parser_new(File* srcfile, Token** tokens) {
    Parser parser;
    parser.srcfile = srcfile;
    parser.tokens = tokens;
    parser.tokens_idx = 0;
    parser.stmts = null;
    parser.decls = null;
    parser.imports = null;
    parser.error_panic = false;
    parser.error_count = 0;
    parser.error_loc = ERRLOC_GLOBAL;
    parser.error_state = ERROR_SUCCESS;
    return parser;
}

#define check_eof \
    if (current(self)->type == T_EOF) return

static void sync_to_next_statement(Parser* self) {
    self->error_state = ERROR_PARSE;

    switch (self->error_loc) {
    case ERRLOC_BLOCK:
    case ERRLOC_FUNCTION_HEADER:
    case ERRLOC_GLOBAL:
        while (!match(self, T_SEMICOLON) && current(self)->type != T_L_BRACE) {
            check_eof;
            goto_next_token(self);
        }
        if (current(self)->type == T_L_BRACE) goto skip_brace;
        break;

    case ERRLOC_NONE: {
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
    self->error_state = ERROR_PARSE;
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

#define EXPR_ND(name, init_func, ...) \
    { \
        es; \
        name = init_func(__VA_ARGS__); \
        ern; \
    }

#define EXPR(name) \
    EXPR_CI(name, expr, self)

static Token* current(Parser* self) {
    if (self->tokens_idx < buf_len(self->tokens)) {
        return self->tokens[self->tokens_idx];
    }
    assert(0);
    return null;
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

static void goto_previous_token(Parser* self) {
    if (self->tokens_idx > 0) {
        self->tokens_idx--;
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

typedef struct {
    DataType* data_type;
    bool error;
} DataTypeError;

static DataTypeError match_data_type(Parser* self) {
    if (match(self, T_IDENTIFIER)) {
        Token* identifier = previous(self);
        u8 pointer_count = 0;
        while (match(self, T_STAR)) {
            if (pointer_count > 128) {
                error_token_with_sync(
                        self,
                        current(self),
                        "pointer-to-pointer limit[128] exceeded"
                );
                return (DataTypeError){ null, true };
            }
            pointer_count++;
        }
        return (DataTypeError){
            data_type_new_alloc(identifier, pointer_count),
            false
        };
    }
    return (DataTypeError){ null, false };
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

#define expect_colon(self) \
    chk(expect(self, T_COLON, "expect ':'"))

#define expect_l_brace(self) \
    chk(expect(self, T_L_BRACE, "expect '{'"))

#define expect_comma(self) \
    chk(expect(self, T_COMMA, "expect ','"))

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

static Stmt* stmt_variable_decl_new(
        Token* identifier,
        DataType* data_type,
        Expr* initializer) {

    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_VARIABLE_DECL;
    stmt->s.variable_decl.identifier = identifier;
    stmt->s.variable_decl.data_type = data_type;
    stmt->s.variable_decl.initializer = initializer;
}

static Stmt* variable_decl(Parser* self, Token* identifier) {
    DataTypeError data_type = match_data_type(self);
    if (data_type.error) return null;
    Expr* initializer = null;
    if (match(self, T_COLON)) {
        EXPR_ND(initializer, expr, self);
    }
    else if (!data_type.data_type) {
        error_token_with_sync(
                self,
                identifier,
                "initializer is required when type is not annotated"
        );
        return null;
    }
    expect_semicolon(self);
    return stmt_variable_decl_new(
            identifier,
            data_type.data_type,
            initializer
    );
}

static Stmt* stmt_expr_new(Expr* expr) {
    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_EXPR;
    stmt->s.expr = expr;
    return stmt;
}

static Stmt* stmt_block_new(Stmt** block) {
    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_BLOCK;
    stmt->s.block = block;
    return stmt;
}

static Stmt* stmt(Parser* self) {
    if (match(self, T_IDENTIFIER)) {
        Token* identifier = previous(self);
        if (match(self, T_COLON)) {
            return variable_decl(self, identifier);
        }
        else {
            goto_previous_token(self);
        }
    }
    else if (match(self, T_L_BRACE)) {
        self->error_loc = ERRLOC_BLOCK;
        Stmt** block = null;
        bool error = false;
        while (!match(self, T_R_BRACE)) {
            check_eof null;
            Stmt* s = stmt(self);
            if (s) buf_push(block, s);
            else error = true;
        }
        if (error) return null;
        else return stmt_block_new(block);
    }
    else {
        EXPR(e);
        expect_semicolon(self);
        return stmt_expr_new(e);
    }
}

static Stmt* stmt_function_def_new(
        Token* identifier,
        Param** params,
        Stmt* body) {

    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_FUNCTION_DEF;
    stmt->s.function_def.identifier = identifier;
    stmt->s.function_def.params = params;
    stmt->s.function_def.body = body;
    return stmt;
}

static Stmt* stmt_function_decl_new(
        Token* identifier,
        Param** params) {

    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_FUNCTION_DECL;
    stmt->s.function_decl.identifier = identifier;
    stmt->s.function_decl.params = params;
    return stmt;
}

static Stmt* decl(Parser* self) {
    self->error_loc = ERRLOC_GLOBAL;
    if (match_keyword(self, "fn")) {
        self->error_loc = ERRLOC_FUNCTION_HEADER;
        expect_identifier(self);
        Token* identifier = previous(self);
        Param** params = null;
        if (match(self, T_L_PAREN)) {
            while (!match(self, T_R_PAREN)) {
                expect_identifier(self);
                Token* p_identifier = previous(self);

                expect_colon(self);
                DataTypeError p_data_type = match_data_type(self);
                if (!p_data_type.data_type && !p_data_type.error) {
                    error_token_with_sync(
                            self,
                            current(self),
                            "expect data type"
                    );
                    return null;
                }
                else if (p_data_type.error) return null;

                if (current(self)->type != T_R_PAREN) {
                    expect_comma(self);
                }
                else match(self, T_COMMA);

                buf_push(
                        params,
                        param_new_alloc(p_identifier, p_data_type.data_type)
                );
            }
        }

        if (match(self, T_L_BRACE)) {
            goto_previous_token(self);

            Stmt* body = stmt(self);
            if (!body) return null;
            buf_push(
                    self->decls,
                    stmt_function_decl_new(identifier, params)
            );
            return stmt_function_def_new(identifier, params, body);
        }
        else {
            expect_semicolon(self);
            return stmt_function_decl_new(identifier, params);
        }
    }
    else if (match(self, T_IDENTIFIER)) {
        Token* identifier = previous(self);
        if (match(self, T_COLON)) {
            return variable_decl(self, identifier);
        }
        else goto error;
    }
    else if (match(self, T_IMPORT)) {
        if (str_intern(current(self)->lexeme) == str_intern("\"\"")) {
            error_token_with_sync(
                    self,
                    current(self),
                    "#import: empty file path"
            );
            return null;
        }
        goto_next_token(self);
        Token* path = previous(self);
        expect_semicolon(self);

        const char* path_without_quotes =
            str_intern_range(path->start + 1, path->end - 1);
        buf_push(self->imports, path_without_quotes);
    }
    else {
    error:
        error_token_with_sync(
                self,
                current(self),
                "expect top-level declaration"
        );
        return null;
    }
}

Error parser_run(Parser* self) {
    while (current(self)->type != T_EOF) {
        Stmt* s = decl(self);
        if (s) buf_push(self->stmts, s);
    }
    return self->error_state;
}

