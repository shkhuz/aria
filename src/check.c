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
static Type* check_symbol_expr(CheckContext* c, Expr* expr);
static Type* check_function_call_expr(CheckContext* c, Expr* expr);
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

        case EXPR_KIND_SYMBOL: {
            return check_symbol_expr(c, expr);
        } break;

        case EXPR_KIND_FUNCTION_CALL: {
            return check_function_call_expr(c, expr);
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

Type* check_symbol_expr(CheckContext* c, Expr* expr) {
    assert(expr->symbol.ref);
    if (expr->symbol.ref->kind != STMT_KIND_FUNCTION) {
        Type* type = stmt_get_type(expr->symbol.ref);
        return type;
    }
    else {
        check_error(
                expr->main_token, 
                "internal_error: function ptr not implemented");
        return null;
    }
}

Type* check_function_call_expr(CheckContext* c, Expr* expr) {
    Expr** args = expr->function_call.args;
    size_t arg_len = buf_len(args);
    assert(expr->function_call.callee->symbol.ref->kind == STMT_KIND_FUNCTION);
    Stmt** params = expr->function_call.callee->symbol.ref->function.header->params;
    size_t param_len = buf_len(params);
    Type* callee_return_type = expr->function_call.callee->symbol.ref->function.header->return_type;

    if (arg_len != param_len) {
        if (arg_len < param_len) {
            check_error(
                    expr->function_call.rparen,
                    "expected argument of type `{t}`",
                    params[arg_len]->param.type);
            note(
                    params[arg_len]->param.identifier,
                    "...parameter declared here");
        }
        else {
            check_error(
                    args[param_len]->main_token,
                    "extra argument(s) supplied");
        }
        return callee_return_type;
    }

    buf_loop(args, i) {
        Type* param_type = params[i]->param.type;
        Type* arg_type = check_expr(c, args[i], param_type);
        if (arg_type && param_type && 
            check_implicit_cast(c, arg_type, param_type) == IMPLICIT_CAST_ERROR) {
            check_error(
                    args[i]->main_token,
                    "argument type `{t}` cannot be converted to `{t}`",
                    arg_type,
                    param_type);
            note(
                    param_type->main_token,
                    "...parameter type annotated here");
        }
    }
    return callee_return_type;
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
