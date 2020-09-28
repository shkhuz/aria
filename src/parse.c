#include "arpch.h"
#include "util/util.h"
#include "ds/ds.h"
#include "error_msg.h"
#include "aria.h"

#define push_stmt(stmt) buf_push(self->ast->stmts, stmt)

#define check_eof \
    if (current(self)->type == T_EOF) return

static bool match(Parser* self, TokenType type);
static Token* current(Parser* self);
static void goto_next_token(Parser* self);
static void goto_previous_token(Parser* self);
static Stmt* variable_decl(Parser* self, Token* identifier);

static void sync_to_next_statement(Parser* self) {
    self->error_state = true;

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

/*     case ERRLOC_STRUCT_FIELD: */
/*         while (!match(self, T_COMMA) && current(self)->type != T_L_BRACE) { */
/*             check_eof; */
/*             goto_next_token(self); */
/*         } */
/*         if (current(self)->type == T_L_BRACE) goto skip_brace; */
/*         break; */

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
    self->error_state = true;
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

#include "error_recover.h"

/* custom initialization */
#define EXPR_CI(name, init_func, ...) \
    Expr* name = null; \
    { \
        es; \
        name = init_func(__VA_ARGS__); \
        ern; \
    }

/* no declaration */
#define EXPR_ND(name, init_func, ...) \
    { \
        es; \
        name = init_func(__VA_ARGS__); \
        ern; \
    }

#define EXPR(name) \
    EXPR_CI(name, expr, self)

static Token* current(Parser* self) {
    if (self->tokens_idx < buf_len(self->ast->tokens)) {
        return self->ast->tokens[self->tokens_idx];
    }
    assert(0);
    return null;
}

static Token* previous(Parser* self) {
    if (self->tokens_idx > 0) {
        return self->ast->tokens[self->tokens_idx - 1];
    }
    assert(0);
    return null;
}

static void goto_next_token(Parser* self) {
    if (self->tokens_idx < buf_len(self->ast->tokens)) {
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

#define expect_identifier_error_msg "expect identifier"
#define expect_colon_error_msg "expect ':'"
#define expect_comma_error_msg "expect ','"
#define expect_semicolon_error_msg "expect ';'"

#define expect_identifier(self) \
    chk(expect(self, T_IDENTIFIER, expect_identifier_error_msg))

#define expect_identifier_lp(self) \
    chklp(expect(self, T_IDENTIFIER, expect_identifier_error_msg))

#define expect_semicolon(self) \
    chk(expect(self, T_SEMICOLON, expect_semicolon_error_msg))

#define expect_colon(self) \
    chk(expect(self, T_COLON, expect_colon_error_msg))

#define expect_colon_lp(self) \
    chklp(expect(self, T_COLON, expect_colon_error_msg))

#define expect_l_brace(self) \
    chk(expect(self, T_L_BRACE, "expect '{'"))

#define expect_l_paren(self) \
    chk(expect(self, T_L_PAREN, "expect '('"))

#define expect_comma(self) \
    chk(expect(self, T_COMMA, expect_comma_error_msg))

#define expect_comma_lp(self) \
    chklp(expect(self, T_COMMA, expect_comma_error_msg))

typedef struct {
    bool is_stmt;
    union {
        Stmt* stmt;
        Expr* expr;
    };
} StmtLevelOutput;

static StmtLevelOutput stmt(Parser* self);

static Expr* expr_integer_new(Token* integer) {
    Expr* expr = expr_new_alloc();
    expr->type = E_INTEGER;
    expr->head = integer;
    expr->tail = integer;
    expr->integer = integer;
    return expr;
}

static Expr* expr_variable_ref_new(Token* identifier) {
    Expr* expr = expr_new_alloc();
    expr->type = E_VARIABLE_REF;
    expr->head = identifier;
    expr->tail = identifier;
    expr->variable_ref.identifier = identifier;
    return expr;
}

static Expr* expr_block_new(
        Stmt** stmts,
        Expr* ret,
        Token* l_brace,
        Token* r_brace) {

    Expr* expr = expr_new_alloc();
    expr->type = E_BLOCK;
    expr->head = l_brace;
    expr->tail = r_brace;
    expr->block.stmts = stmts;
    expr->block.ret = ret;
    return expr;
}

static Expr* expr_assign_new(Expr* left, Expr* right) {
    Expr* expr = expr_new_alloc();
    expr->type = E_ASSIGN;
    expr->head = left->head;
    expr->tail = right->tail;
    expr->assign.left = left;
    expr->assign.right = right;
    return expr;
}

static Expr* expr_atom(Parser* self) {
    if (match(self, T_INTEGER)) {
        return expr_integer_new(previous(self));
    }
    else if (match(self, T_IDENTIFIER)) {
        return expr_variable_ref_new(previous(self));
    }
    else if (match(self, T_L_BRACE)) {
        Token* l_brace = previous(self);
        self->error_loc = ERRLOC_BLOCK;
        Stmt** block = null;
        Expr* ret = null;
        bool error = false;

        while (!match(self, T_R_BRACE)) {
            check_eof null;
            StmtLevelOutput s = stmt(self);
            if (s.is_stmt) {
                if (s.stmt) buf_push(block, s.stmt);
                else error = true;
            }
            else {
                ret = s.expr;
            }
        }
        Token* r_brace = previous(self);

        if (error) return null;
        else return expr_block_new(block, ret, l_brace, r_brace);
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

static Expr* expr_assign(Parser* self) {
    EXPR_CI(left, expr_atom, self);
    if (match(self, T_EQUAL)) {
        EXPR_CI(right, expr_assign, self);
        if (left->type == E_VARIABLE_REF) {
            return expr_assign_new(left, right);
        }
        else {
            error_expr(
                    left,
                    "invalid assignment target"
            );
            return null;
        }
    }
    return left;
}

static Expr* expr(Parser* self) {
    return expr_assign(self);
}

static Stmt* stmt_expr_new(Expr* expr) {
    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_EXPR;
    stmt->expr = expr;
    return stmt;
}

static Stmt* stmt_return_new(Expr* expr) {
    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_RETURN;
    stmt->ret = expr;
    return stmt;
}

#define expect_semicolon_custom(self) \
    es; expect(self, T_SEMICOLON, expect_semicolon_error_msg); er out;
/* --- */
static StmtLevelOutput stmt(Parser* self) {
    StmtLevelOutput out;
    out.is_stmt = true;
    out.stmt = null;
    if (match_keyword(self, "return")) {
        Expr* e = null;
        { es; e = expr(self); er out; }
        out.stmt = stmt_return_new(e);
        expect_semicolon_custom(self);
        return out;
    }

    else if (match(self, T_IDENTIFIER)) {
        Token* identifier = previous(self);
        if (match(self, T_COLON)) {
            out.stmt = variable_decl(self, identifier);
            return out;
        }
        else {
            goto_previous_token(self);
        }
    }

    Expr* e = null;
    { es; e = expr(self); er out; }
    if (current(self)->type == T_R_BRACE) {
        out.is_stmt = false;
        out.expr = e;
        return out;
    }
    else {
        if (e->type != E_BLOCK) {
            expect_semicolon_custom(self);
        }
        out.stmt = stmt_expr_new(e);
        return out;
    }
}
/* --- */
#undef expect_semicolon_custom

static Stmt* stmt_variable_decl_new(
        Token* identifier,
        DataType* data_type,
        Expr* initializer,
        bool global,
        bool external) {

    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_VARIABLE_DECL;
    stmt->variable_decl.identifier = identifier;
    stmt->variable_decl.data_type = data_type;
    stmt->variable_decl.initializer = initializer;
    stmt->variable_decl.global = global;
    stmt->variable_decl.external = external;
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

    return stmt_variable_decl_new(
            identifier,
            data_type.data_type,
            initializer,
            !self->in_function,
            false
    );
}

static Stmt* stmt_function_new(
        Token* identifier,
        Stmt** params,
        DataType* return_type,
        Expr* block,
        bool decl) {

    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_FUNCTION;
    stmt->function.identifier = identifier;
    stmt->function.params = params;
    stmt->function.return_type = return_type;
    stmt->function.block = block;
    stmt->function.decl = decl;
    return stmt;
}

static Stmt* decl(Parser* self) {
    self->error_loc = ERRLOC_GLOBAL;

    if (match_keyword(self, "fn")) {
        self->error_loc = ERRLOC_FUNCTION_HEADER;
        expect_identifier(self);
        Token* identifier = previous(self);
        Stmt** params = null;
        expect_l_paren(self);

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

        DataTypeError return_type = match_data_type(self);
        if (return_type.error) return null;
        if (!return_type.data_type) {
            return_type.data_type = builtin_types.e.void_type;
        }

        if (match(self, T_L_BRACE)) {
            goto_previous_token(self);

            self->in_function = true;
            Expr* block = expr(self);
            self->in_function = false;
            if (!block) return null;

            /* buf_push( */
            /*         self->decls, */
            /*         stmt_function_new( */
            /*             identifier, */
            /*             params, */
            /*             return_type.data_type, */
            /*             null, */
            /*             true */
            /*         ) */
            /* ); */
            return stmt_function_new(
                    identifier,
                    params,
                    return_type.data_type,
                    block,
                    false
            );
        }

        // function declaration
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

    else if (match(self, T_IDENTIFIER)) {
        Token* identifier = previous(self);
        if (match(self, T_COLON)) {
            return variable_decl(self, identifier);
        }
        else goto error;
    }

error:
    error_token_with_sync(
            self,
            current(self),
            "expect top-level declaration"
    );
    return null;
}

AstOutput parse(Parser* self, File* srcfile, Token** tokens) {
    self->ast = malloc(sizeof(Ast));
    self->ast->srcfile = srcfile;
    self->ast->tokens = tokens;
    self->ast->stmts = null;
    self->ast->decls = null;
    self->ast->imports = null;

    self->tokens_idx = 0;
    self->in_function = false;
    self->error_panic = false;
    self->error_count = 0;
    self->error_loc = ERRLOC_NONE;
    self->error_state = false;

    while (current(self)->type != T_EOF) {
        Stmt* s = decl(self);
        if (s) push_stmt(s);
    }
    return (AstOutput){ self->error_state, self->ast };
}

