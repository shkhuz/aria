#include "arpch.h"
#include "util/util.h"
#include "ds/ds.h"
#include "error_msg.h"
#include "aria.h"

#define push_stmt(stmt) buf_push(self->ast->stmts, stmt)

#define check_eof \
    if (cur(self)->type == T_EOF) return

static bool match(Parser* self, TokenType type);
static Token* cur(Parser* self);
static void goto_next_token(Parser* self);
static void goto_previous_token(Parser* self);
static Stmt* variable_decl(Parser* self, Token* identifier);

static void sync_to_next_statement(Parser* self) {
    self->error_state = true;

    switch (self->error_loc) {
    case ERRLOC_BLOCK:
    case ERRLOC_FUNCTION_HEADER:
    case ERRLOC_GLOBAL:
        while (!match(self, T_SEMICOLON) && cur(self)->type != T_L_BRACE) {
            check_eof;
            goto_next_token(self);
        }
        if (cur(self)->type == T_L_BRACE) goto skip_brace;
        break;

/*     case ERRLOC_STRUCT_FIELD: */
/*         while (!match(self, T_COMMA) && cur(self)->type != T_L_BRACE) { */
/*             check_eof; */
/*             goto_next_token(self); */
/*         } */
/*         if (cur(self)->type == T_L_BRACE) goto skip_brace; */
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
            while (match(self, T_SEMICOLON));
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
    vmsg_token(
            MSG_ERROR,
            token,
            fmt,
            ap
    );
    sync_to_next_statement(self);
    va_end(ap);
}

static void error_expr_with_sync(
        Parser* self,
        Expr* expr,
        const char* fmt,
        ...) {

    self->error_count++;
    self->error_state = true;
    va_list ap;
    va_start(ap, fmt);
    vmsg_expr(
            MSG_ERROR,
            expr,
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

/* custom initialization; continue if error */
#define EXPR_CI_LP(name, init_func, ...) \
    Expr* name = null; \
    { \
        es; \
        name = init_func(__VA_ARGS__); \
        ec; \
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

static Token* cur(Parser* self) {
    if (self->tokens_idx < buf_len(self->ast->tokens)) {
        return self->ast->tokens[self->tokens_idx];
    }
    assert(0);
    return null;
}

static Token* prev(Parser* self) {
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
    if (cur(self)->type == type) {
        goto_next_token(self);
        return true;
    }
    return false;
}

static bool match_keyword(Parser* self, const char* keyword) {
    if (cur(self)->type == T_KEYWORD &&
        str_intern(cur(self)->lexeme) == str_intern((char*)keyword)) {
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
        Token* identifier = prev(self);
        u8 pointer_count = 0;
        while (match(self, T_STAR)) {
            if (pointer_count > 128) {
                error_token_with_sync(
                        self,
                        cur(self),
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
        error_token_with_sync(self, cur(self), fmt, ap);
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

static Expr* expr_binary_new(Expr* left, Expr* right, Token* op) {
    Expr* expr = expr_new_alloc();
    expr->type = E_BINARY;
    expr->head = left->head;
    expr->tail = right->tail;
    expr->binary.left = left;
    expr->binary.right = right;
    expr->binary.op = op;
    return expr;
}

static Expr* expr_unary_new(Token* op, Expr* right) {
    Expr* expr = expr_new_alloc();
    expr->type = E_UNARY;
    expr->head = op;
    expr->tail = right->tail;
    expr->unary.op = op;
    expr->unary.right = right;
    return expr;
}

static Expr* expr_func_call_new(Expr* left, Expr** args, Token* r_paren) {
    Expr* expr = expr_new_alloc();
    expr->type = E_FUNC_CALL;
    expr->head = left->head;
    expr->tail = r_paren;
    expr->func_call.left = left;
    expr->func_call.args = args;
    return expr;
}

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

static Expr* expr_string_new(Token* string) {
    Expr* expr = expr_new_alloc();
    expr->type = E_STRING;
    expr->head = string;
    expr->tail = string;
    expr->string = string;
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

static Expr* expr_assign(Parser* self);

static Expr* expr_atom(Parser* self) {
    if (match(self, T_INTEGER)) {
        return expr_integer_new(prev(self));
    }
    else if (match(self, T_IDENTIFIER)) {
        return expr_variable_ref_new(prev(self));
    }
    else if (match(self, T_STRING)) {
        return expr_string_new(prev(self));
    }
    else if (match(self, T_L_BRACE)) {
        Token* l_brace = prev(self);
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
        Token* r_brace = prev(self);

        if (error) return null;
        else return expr_block_new(block, ret, l_brace, r_brace);
    }
    else {
        error_token_with_sync(
                self,
                cur(self),
                "invalid ast: `%s` is not expected here",
                cur(self)->lexeme
        );
        return null;
    }
}

static Expr* expr_postfix(Parser* self) {
    EXPR_CI(left, expr_atom, self);
    while (match(self, T_L_PAREN)) {
        if (prev(self)->type == T_L_PAREN) {
            /* func call */
            if (left->type != E_VARIABLE_REF) {
                error_expr_with_sync(
                        self,
                        left,
                        "invalid operand to call operator"
                );
                return null;
            }

            Expr** args = null;
            while (!match(self, T_R_PAREN)) {
                EXPR_CI_LP(arg, expr_assign, self);
                if (arg) buf_push(args, arg);
                if (cur(self)->type != T_R_PAREN) {
                    expect_comma(self);
                }
                else match(self, T_COMMA);
            }
            left = expr_func_call_new(left, args, prev(self));
        }
    }
    return left;
}

static Expr* expr_unary(Parser* self) {
    if (match(self, T_AMPERSAND)) {
        Token* op = prev(self);
        EXPR_CI(right, expr_unary, self);
        if (right->type == E_VARIABLE_REF) {
            return expr_unary_new(op, right);
        }
        else {
            error_expr_with_sync(
                    self,
                    right,
                    "invalid operand to `&`: lvalue required"
            );
            return null;
        }
    }
    return expr_postfix(self);
}

static Expr* expr_binary_add_sub(Parser* self) {
    EXPR_CI(left, expr_unary, self);
    while (match(self, T_PLUS) ||
           match(self, T_MINUS)) {
        Token* op = prev(self);
        EXPR_CI(right, expr_unary, self);
        left = expr_binary_new(left, right, op);
    }
    return left;
}

static Expr* expr_assign(Parser* self) {
    EXPR_CI(left, expr_binary_add_sub, self);
    if (match(self, T_EQUAL)) {
        Token* op = prev(self);
        EXPR_CI(right, expr_assign, self);
        if (left->type == E_VARIABLE_REF) {
            return expr_binary_new(left, right, op);
        }
        else {
            error_expr_with_sync(
                    self,
                    left,
                    "invalid assignment target: lvalue required"
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

static Stmt* stmt_return_new(Token* return_keyword, Expr* expr) {
    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_RETURN;
    stmt->ret.return_keyword = return_keyword;
    stmt->ret.expr = expr;
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
        Token* return_keyword = prev(self);
        Expr* e = null;
        if (!match(self, T_SEMICOLON)) {
            { es; e = expr(self); er out; }
            out.stmt = stmt_return_new(return_keyword, e);
            expect_semicolon_custom(self);
        } else {
            out.stmt = stmt_return_new(return_keyword, null);
        }
        return out;
    }

    else if (match(self, T_IDENTIFIER)) {
        Token* identifier = prev(self);
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
    if (cur(self)->type == T_R_BRACE) {
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
        bool external,
        bool param) {

    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_VARIABLE_DECL;
    stmt->variable_decl.identifier = identifier;
    stmt->variable_decl.data_type = data_type;
    stmt->variable_decl.initializer = initializer;
    stmt->variable_decl.global = global;
    stmt->variable_decl.external = external;
    stmt->variable_decl.param = param;
    return stmt;
}

static Stmt* variable_decl(Parser* self, Token* identifier) {
    DataTypeError data_type = match_data_type(self);
    if (data_type.error) return null;
    Expr* initializer = null;
    if (match(self, T_COLON)) {
        EXPR_ND(initializer, expr, self);
    }
    else if (cur(self)->type != T_SEMICOLON) {
        error_token_with_sync(
                self,
                cur(self),
                "expect `:` before initializer"
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
            false,
            false
    );
}

static Stmt* stmt_function_new(
        Token* identifier,
        Stmt** params,
        DataType* return_type,
        Expr* block,
        bool decl,
        bool pub) {

    Stmt* stmt = stmt_new_alloc();
    stmt->type = S_FUNCTION;
    stmt->function.identifier = identifier;
    stmt->function.params = params;
    stmt->function.variable_decls = null;
    stmt->function.return_type = return_type;
    stmt->function.block = block;
    stmt->function.decl = decl;
    stmt->function.pub = pub;
    return stmt;
}

static Stmt* decl(Parser* self) {
    self->error_loc = ERRLOC_GLOBAL;

    bool pub = false;
    if (match_keyword(self, "pub")) {
        pub = true;
    }

    if (match_keyword(self, "fn")) {
        self->error_loc = ERRLOC_FUNCTION_HEADER;
        expect_identifier(self);
        Token* identifier = prev(self);
        Stmt** params = null;
        expect_l_paren(self);

        while (!match(self, T_R_PAREN)) {
            expect_identifier(self);
            Token* p_identifier = prev(self);

            expect_colon(self);
            DataTypeError p_data_type = match_data_type(self);
            if (!p_data_type.data_type && !p_data_type.error) {
                error_token_with_sync(
                        self,
                        cur(self),
                        "expect data type"
                );
                return null;
            }
            else if (p_data_type.error) return null;

            if (cur(self)->type != T_R_PAREN) {
                expect_comma(self);
            }
            else match(self, T_COMMA);

            Stmt* param =
                stmt_variable_decl_new(
                        p_identifier,
                        p_data_type.data_type,
                        null,
                        false,
                        false,
                        true
                );
            buf_push(params, param);
        }

        DataTypeError return_type;
        return_type.data_type = builtin_types.e.void_type;
        if (match(self, T_COLON)) {
            return_type = match_data_type(self);
            if (return_type.error) return null;
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
                    false,
                    pub
            );
        }

        if (cur(self)->type != T_SEMICOLON) {
            error_token_with_sync(
                    self,
                    cur(self),
                    "expected `:`, `;` or `{`"
            );
            return null;
        }

        /* function declaration */
        else {
            expect_semicolon(self);
            return stmt_function_new(
                    identifier,
                    params,
                    return_type.data_type,
                    null,
                    true,
                    pub
            );
        }
    }

    else if (match(self, T_IDENTIFIER)) {
        Token* identifier = prev(self);
        if (match(self, T_COLON)) {
            return variable_decl(self, identifier);
        }
        else goto error;
    }

error:
    error_token_with_sync(
            self,
            cur(self),
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

    while (cur(self)->type != T_EOF) {
        Stmt* s = decl(self);
        if (s) push_stmt(s);
    }
    return (AstOutput){ self->error_state, self->ast };
}

