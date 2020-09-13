#include <parser.h>
#include <ast_print.h>
#include <token.h>
#include <data_type.h>
#include <expr.h>
#include <builtin_types.h>
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
    parser.in_function = false;
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

    case ERRLOC_STRUCT_FIELD:
        while (!match(self, T_COMMA) && current(self)->type != T_L_BRACE) {
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
    default: assert(0); break;
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

#define ec \
    if (self->error_count > COMBINE(_error_store,__LINE__)) continue

#define ern \
    er null

#define chk(stmt) \
    es; stmt; ern;

#define chklp(stmt) \
    es; stmt; ec;

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
        str_intern(current(self)->lexeme) == str_intern((char*)keyword)) {
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

/* static void expect_keyword(Parser* self, const char* keyword) { */
/*     if (!match_keyword(self, keyword)) { */
/*         error_token_with_sync( */
/*                 self, */
/*                 current(self), */
/*                 "expect keyword: `%s`", */
/*                 keyword */
/*         ); */
/*     } */
/* } */

#define expect_identifier_error_msg "expect identifier"
#define expect_colon_error_msg "expect ':'"
#define expect_comma_error_msg "expect ','"

#define expect_identifier(self) \
    chk(expect(self, T_IDENTIFIER, expect_identifier_error_msg))

#define expect_identifier_lp(self) \
    chklp(expect(self, T_IDENTIFIER, expect_identifier_error_msg))

#define expect_semicolon(self) \
    chk(expect(self, T_SEMICOLON, "expect ';'"))

#define expect_colon(self) \
    chk(expect(self, T_COLON, expect_colon_error_msg))

#define expect_colon_lp(self) \
    chklp(expect(self, T_COLON, expect_colon_error_msg))

#define expect_l_brace(self) \
    chk(expect(self, T_L_BRACE, "expect '{'"))

#define expect_comma(self) \
    chk(expect(self, T_COMMA, expect_comma_error_msg))

#define expect_comma_lp(self) \
    chklp(expect(self, T_COMMA, expect_comma_error_msg))

static Expr* expr_variable_ref_new(Token* identifier) {
    Expr* expr = expr_new_alloc();
    expr->type = E_VARIABLE_REF;
    expr->head = identifier;
    expr->tail = identifier;
    expr->e.variable_ref.identifier = identifier;
    expr->e.variable_ref.variable_ref = null;
    return expr;
}

static Expr* expr_integer_new(Token* integer) {
    Expr* expr = expr_new_alloc();
    expr->type = E_INTEGER;
    expr->head = integer;
    expr->tail = integer;
    expr->e.integer = integer;
    return expr;
}

static Expr* expr_string_new(Token* str) {
    Expr* expr = expr_new_alloc();
    expr->type = E_STRING;
    expr->head = str;
    expr->tail = str;
    expr->e.str = str;
    return expr;
}

static Expr* expr_char_new(Token* chr) {
    Expr* expr = expr_new_alloc();
    expr->type = E_CHAR;
    expr->head = chr;
    expr->tail = chr;
    expr->e.chr = chr;
    return expr;
}

static Expr* expr_unary_new(Token* op, Expr* right) {
    Expr* expr = expr_new_alloc();
    expr->type = E_UNARY;
    expr->head = op;
    expr->tail = right->tail;
    expr->e.unary.op = op;
    expr->e.unary.right = right;
    return expr;
}

static Expr* expr_binary_new(Expr* left, Expr* right, Token* op) {
    Expr* expr = expr_new_alloc();
    expr->type = E_BINARY;
    expr->head = left->head;
    expr->tail = right->tail;
    expr->e.binary.left = left;
    expr->e.binary.right = right;
    expr->e.binary.op = op;
    return expr;
}

static Expr* expr_atom(Parser* self) {
    if (match(self, T_IDENTIFIER)) {
        return expr_variable_ref_new(previous(self));
    }
    else if (match(self, T_INTEGER)) {
        return expr_integer_new(previous(self));
    }
    else if (match(self, T_STRING)) {
        return expr_string_new(previous(self));
    }
    else if (match(self, T_CHAR)) {
        return expr_char_new(previous(self));
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
        Expr* initializer,
        bool global,
        bool external) {

    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_VARIABLE_DECL;
    stmt->s.variable_decl.identifier = identifier;
    stmt->s.variable_decl.data_type = data_type;
    stmt->s.variable_decl.initializer = initializer;
    stmt->s.variable_decl.global = global;
    stmt->s.variable_decl.external = external;
    return stmt;
}

static Stmt* variable_decl(Parser* self, Token* identifier) {
    DataTypeError data_type = match_data_type(self);
    if (data_type.error) return null;
    Expr* initializer = null;
    if (match(self, T_EQUAL)) {
        EXPR_ND(initializer, expr, self);
    }
    else if (current(self)->type != T_SEMICOLON) {
        error_token_with_sync(
                self,
                current(self),
                "expect `=`"
        );
        return null;
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

    if (!self->in_function) {
        Stmt* variable =
            stmt_variable_decl_new(
                identifier,
                data_type.data_type,
                null,
                true,
                true
            );
        buf_push(self->decls, variable);
    }

    return stmt_variable_decl_new(
            identifier,
            data_type.data_type,
            initializer,
            !self->in_function,
            false
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

    EXPR(e);
    expect_semicolon(self);
    return stmt_expr_new(e);
}

static Stmt* stmt_function_new(
        Token* identifier,
        Stmt** params,
        DataType* return_type,
        Stmt* body,
        bool decl) {

    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_FUNCTION;
    stmt->s.function.identifier = identifier;
    stmt->s.function.params = params;
    stmt->s.function.return_type = return_type;
    stmt->s.function.body = body;
    stmt->s.function.decl = decl;
    return stmt;
}

static Stmt* stmt_struct_new(Token* identifier, Stmt** fields, bool external) {
    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_STRUCT;
    stmt->s.struct_decl.identifier = identifier;
    stmt->s.struct_decl.fields = fields;
    stmt->s.struct_decl.external = external;
    return stmt;
}

static Stmt* decl(Parser* self) {
    self->error_loc = ERRLOC_GLOBAL;
    if (match_keyword(self, "fn")) {
        self->error_loc = ERRLOC_FUNCTION_HEADER;

        expect_identifier(self);
        Token* identifier = previous(self);
        Stmt** params = null;
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

                Stmt* param =
                    stmt_variable_decl_new(
                            p_identifier,
                            p_data_type.data_type,
                            null,
                            false,
                            false
                    );
                buf_push(params, param);
            }
        }

        DataTypeError return_type = match_data_type(self);
        if (return_type.error) return null;
        if (!return_type.data_type) {
            return_type.data_type = builtin_types.e.void_type;
        }

        if (match(self, T_L_BRACE)) {
            goto_previous_token(self);

            self->in_function = true;
            Stmt* body = stmt(self);
            self->in_function = false;
            if (!body) return null;

            buf_push(
                    self->decls,
                    stmt_function_new(
                        identifier,
                        params,
                        return_type.data_type,
                        null,
                        true
                    )
            );
            return stmt_function_new(
                    identifier,
                    params,
                    return_type.data_type,
                    body,
                    false
            );
        }
        else {
            expect_semicolon(self);
            return stmt_function_new(
                    identifier,
                    params,
                    return_type.data_type,
                    null,
                    true
            );
        }
    }

    else if (match_keyword(self, "struct")) {
        expect_identifier(self);
        Token* identifier = previous(self);

        expect_l_brace(self);
        self->error_loc = ERRLOC_STRUCT_FIELD;

        Stmt** fields = null;
        while (!match(self, T_R_BRACE)) {
            check_eof null;
            expect_identifier_lp(self);
            Token* f_identifier = previous(self);
            expect_colon_lp(self);

            DataTypeError dt = match_data_type(self);
            if (dt.error) continue;
            if (!dt.data_type) {
                error_token_with_sync(
                        self,
                        current(self),
                        "expect field data-type"
                );
                continue;
            }

            if (current(self)->type != T_R_BRACE) {
                expect_comma_lp(self);
            }
            else match(self, T_COMMA);

            Stmt* field =
                stmt_variable_decl_new(
                        f_identifier,
                        dt.data_type,
                        null,
                        false,
                        false
                );
            buf_push(fields, field);
        }

        Stmt* struct_stmt = stmt_struct_new(identifier, fields, true);
        buf_push(self->decls, struct_stmt);
        return stmt_struct_new(identifier, fields, false);
    }

    else if (match(self, T_IDENTIFIER)) {
        Token* identifier = previous(self);
        if (match(self, T_COLON)) {
            return variable_decl(self, identifier);
        }
        else goto error;
    }

    else if (match(self, T_IMPORT)) {
        if (current(self)->type != T_STRING) {
            error_token_with_sync(
                    self,
                    current(self),
                    "expect compile-time string"
            );
            return null;
        }
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
        return null;
    }
error:
    error_token_with_sync(
            self,
            current(self),
            "expect top-level declaration"
    );
    return null;
}

Error parser_run(Parser* self) {
    while (current(self)->type != T_EOF) {
        Stmt* s = decl(self);
        if (s) buf_push(self->stmts, s);
    }
    return self->error_state;
}

