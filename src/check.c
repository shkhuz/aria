#include "check.h"
#include "buf.h"
#include "stri.h"
#include "msg.h"

#define check_error(token, fmt, ...) \
    error(token, fmt, ##__VA_ARGS__); \
    c->error = true

typedef enum {
    IMPLICIT_CAST_OK,
    IMPLICIT_CAST_ERROR,
} ImplicitCastStatus;

static void check_stmt(CheckContext* c, Stmt* stmt);
static void check_function_stmt(CheckContext* c, Stmt* stmt);
static void check_variable_stmt(CheckContext* c, Stmt* stmt);
static void check_expr_stmt(CheckContext* c, Stmt* stmt);
static Type* check_expr(CheckContext* c, Expr* expr, Type* cast);
static Type* check_integer_expr(CheckContext* c, Expr* expr, Type* cast);
static Type* check_block_expr(CheckContext* c, Expr* expr, Type* cast);
static ImplicitCastStatus check_implicit_cast(
        CheckContext* c, 
        Type* from, 
        Type* to);

void check(CheckContext* c) {
    c->error = false;

    buf_loop(c->srcfile->stmts, i) {
        check_stmt(c, c->srcfile->stmts[i]);
    }
}

void check_stmt(CheckContext* c, Stmt* stmt) {
    switch (stmt->kind) {
        case STMT_KIND_FUNCTION: {
            check_function_stmt(c, stmt);
        } break;

        case STMT_KIND_VARIABLE: {
            check_variable_stmt(c, stmt);
        } break;

        case STMT_KIND_EXPR: {
            check_expr_stmt(c, stmt);
        } break;
    }
}

void check_function_stmt(CheckContext* c, Stmt* stmt) {
    Type* body_type = check_block_expr(
            c, 
            stmt->function.body, 
            stmt->function.header->return_type);
    printf(
            "%s() stack size: %lu\n", 
            stmt->function.header->identifier->lexeme,
            stmt->function.stack_vars_size);
    Type* return_type = stmt->function.header->return_type;
    if (body_type && return_type) {
        if (check_implicit_cast(c, body_type, return_type) == 
                IMPLICIT_CAST_ERROR) {
            check_error(
                    stmt->function.body->main_token,
                    "function annotated `{t}`, but returned `{t}`",
                    return_type,
                    body_type);
        }
    }
}

void check_variable_stmt(CheckContext* c, Stmt* stmt) {
    if (stmt->variable.type && stmt->variable.initializer) {
        Type* initializer_type = check_expr(
                c,
                stmt->variable.initializer,
                stmt->variable.type);
        Type* annotated_type = stmt->variable.type;
        if (initializer_type && annotated_type) {
            if (check_implicit_cast(c, initializer_type, annotated_type) == 
                IMPLICIT_CAST_ERROR) {
                check_error(
                        stmt->variable.identifier,
                        "cannot initialize from `{t}` to `{t}`",
                        initializer_type,
                        annotated_type);
            }
        }
    }
    else if (stmt->variable.initializer) {
        stmt->variable.type = check_expr(
                c, 
                stmt->variable.initializer,
                null);
    }

    if (stmt->variable.parent_func) {
        stmt->variable.parent_func->function.stack_vars_size += 
            type_bytes(stmt->variable.type);
    }
}

void check_expr_stmt(CheckContext* c, Stmt* stmt) {
    check_expr(c, stmt->expr.child, builtin_type_placeholders.void_type);
}

Type* check_expr(CheckContext* c, Expr* expr, Type* cast) {
    switch (expr->kind) {
        case EXPR_KIND_INTEGER: {
            return check_integer_expr(c, expr, cast);
        } break;

        case EXPR_KIND_BLOCK: {
            return check_block_expr(c, expr, cast);
        } break;
    }
    assert(0);
    return null;
}

Type* check_integer_expr(CheckContext* c, Expr* expr, Type* cast) {
    Type* cast_def = cast;
    if (!cast_def) cast_def = builtin_type_placeholders.i32;

    if (type_is_integer(cast_def) && bigint_fits(
                expr->integer.val, 
                builtin_type_bytes(cast_def->builtin.kind), 
                builtin_type_is_signed(cast_def->builtin.kind))) {
        return cast_def;
    }
    else {
        check_error(
                expr->integer.integer,
                "integer cannot be converted to `{t}`",
                cast);
    }
    return null;
}

Type* check_block_expr(CheckContext* c, Expr* expr, Type* cast) {
    buf_loop(expr->block.stmts, i) {
        check_stmt(c, expr->block.stmts[i]);
    }
    if (expr->block.value) {
        return check_expr(c, expr->block.value, cast);
    }
    return builtin_type_placeholders.void_type;
}

ImplicitCastStatus check_implicit_cast(
        CheckContext* c, 
        Type* from, 
        Type* to) {

    assert(from && to);
    if (from->kind == to->kind) {
        if (from->kind == TYPE_KIND_BUILTIN) {
            if (builtin_type_is_integer(from->builtin.kind) &&
                builtin_type_is_integer(to->builtin.kind)) {
                if (builtin_type_bytes(from->builtin.kind) > 
                    builtin_type_bytes(to->builtin.kind) ||
                    builtin_type_is_signed(from->builtin.kind) != 
                    builtin_type_is_signed(to->builtin.kind)) {
                    return IMPLICIT_CAST_ERROR;
                }
                else {
                    return IMPLICIT_CAST_OK;
                }
            }
            else if (from->builtin.kind == to->builtin.kind) {
                if (from->builtin.kind == BUILTIN_TYPE_KIND_BOOLEAN ||
                    from->builtin.kind == BUILTIN_TYPE_KIND_VOID) {
                    return IMPLICIT_CAST_OK;
                } 
                else {
                    return IMPLICIT_CAST_ERROR;
                }
            }
        }
        else if (from->kind == TYPE_KIND_PTR) {
            // There are four cases:
            //           to           C = const
            //      +---+-----+       M = mutable
            //      |   | C M |       O = okay to cast
            //      +---+-----+       ! = cannot cast
            // from | C | O ! |
            //      | M | O O |
            //      +---+-----+
            // 
            // These four cases are handled by this one line:
            if (!from->ptr.constant || to->ptr.constant) {
                return check_implicit_cast(
                        c,
                        type_get_child(from),
                        type_get_child(to));
            }
        }
    }
    return IMPLICIT_CAST_ERROR;
}
