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
static Type* check_constant_expr(CheckContext* c, Expr* expr, Type* cast);
static Type* check_symbol_expr(CheckContext* c, Expr* expr, Type* cast);
static Type* check_function_call_expr(CheckContext* c, Expr* expr, Type* cast);
static Type* check_binop_expr(CheckContext* c, Expr* expr, Type* cast);
static Type* check_block_expr(CheckContext* c, Expr* expr, Type* cast);
static Type* check_if_expr(CheckContext* c, Expr* expr, Type* cast);
static Type* check_if_branch(CheckContext* c, IfBranch* br, Type* cast);
static Type* check_while_expr(CheckContext* c, Expr* expr, Type* cast);
static ImplicitCastStatus check_implicit_cast(
        CheckContext* c, 
        Type* from, 
        Type* to);

void check(CheckContext* c) {
    c->error = false;
    c->last_stack_offset = 0;

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
    c->last_stack_offset = 0;
    if (!stmt->function.is_extern) {
        Type* body_type = check_block_expr(
                c, 
                stmt->function.body, 
                stmt->function.header->return_type);
        Type* return_type = stmt->function.header->return_type;
        stmt->function.body->type = body_type;
        if (body_type && return_type) {
            if (check_implicit_cast(c, body_type, return_type) == IMPLICIT_CAST_ERROR) {
                Token* errtok = null;
                if (stmt->function.body->block.value) {
                    errtok = stmt->function.body->block.value->main_token;
                }
                else {
                    errtok = stmt->function.header->return_type->main_token;
                }
                check_error(
                        errtok,
                        "function annotated `{t}`, but returned `{t}`",
                        return_type,
                        body_type);
            }
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
        stmt->variable.initializer_type = initializer_type;
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
        stmt->variable.initializer_type = stmt->variable.type;
    }

    if (stmt->parent_func && stmt->variable.type) {
        size_t bytes = type_bytes(stmt->variable.type);
        c->last_stack_offset += bytes;
        c->last_stack_offset = 
            round_to_next_multiple(c->last_stack_offset, bytes);
        stmt->variable.stack_offset = c->last_stack_offset;
        stmt->parent_func->function.stack_vars_size = 
            c->last_stack_offset;
    }
}

void check_expr_stmt(CheckContext* c, Stmt* stmt) {
    check_expr(c, stmt->expr.child, null);
}

Type* check_expr(CheckContext* c, Expr* expr, Type* cast) {
    switch (expr->kind) {
        case EXPR_KIND_INTEGER: {
            return check_integer_expr(c, expr, cast);
        } break;

        case EXPR_KIND_CONSTANT: {
            return check_constant_expr(c, expr, cast);
        } break;

        case EXPR_KIND_SYMBOL: {
            return check_symbol_expr(c, expr, cast);
        } break;

        case EXPR_KIND_FUNCTION_CALL: {
            return check_function_call_expr(c, expr, cast);
        } break;

        case EXPR_KIND_BINOP: {
            return check_binop_expr(c, expr, cast);
        } break;

        case EXPR_KIND_BLOCK: {
            return check_block_expr(c, expr, cast);
        } break;

        case EXPR_KIND_IF: {
            return check_if_expr(c, expr, cast);
        } break;

        case EXPR_KIND_WHILE: {
            return check_while_expr(c, expr, cast);
        } break;
    }
    assert(0);
    return null;
}

Type* check_integer_expr(CheckContext* c, Expr* expr, Type* cast) {
    if (!cast) cast = builtin_type_placeholders.i32;
    if (type_is_integer(cast) && bigint_fits(
                expr->integer.val, 
                builtin_type_bytes(cast->builtin.kind), 
                builtin_type_is_signed(cast->builtin.kind))) {
        return cast;
    }
    else {
        check_error(
                expr->integer.integer,
                "integer cannot be converted to `{t}`",
                cast);
    }
    return null;
}

Type* check_constant_expr(CheckContext* c, Expr* expr, Type* cast) {
    Type* type = null;
    switch (expr->constant.kind) {
        case CONSTANT_KIND_BOOLEAN_TRUE:
        case CONSTANT_KIND_BOOLEAN_FALSE: {
            type = builtin_type_placeholders.boolean;
        } break;

        case CONSTANT_KIND_NULL: {
            type = builtin_type_placeholders.void_ptr;
        } break;
    }
    expr->type = cast;
    return type;
}

Type* check_symbol_expr(CheckContext* c, Expr* expr, Type* cast) {
    assert(expr->symbol.ref);
    if (expr->symbol.ref->kind != STMT_KIND_FUNCTION) {
        expr->type = cast;
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

Type* check_function_call_expr(CheckContext* c, Expr* expr, Type* cast) {
    Expr** args = expr->function_call.args;
    size_t arg_len = buf_len(args);
    assert(expr->function_call.callee->symbol.ref->kind == STMT_KIND_FUNCTION);

    Stmt** params = expr->function_call.callee->symbol.ref->function.header->params;
    size_t param_len = buf_len(params);
    Type* callee_return_type = expr->function_call.callee->symbol.ref->function.header->return_type;
    expr->type = cast;

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
        /* args[i]->type = arg_type; */
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

Type* check_binop_expr(CheckContext* c, Expr* expr, Type* cast) {
    Type* left_type = check_expr(c, expr->binop.left, null);
    expr->binop.left_type = left_type;
    Type* right_type = check_expr(c, expr->binop.right, null);
    expr->binop.right_type = right_type;
    expr->type = cast;

    if (left_type && right_type) {
        if (type_is_integer(left_type) && type_is_integer(right_type)) {
            ImplicitCastStatus status;
            bool return_left = true;
            if (type_bytes(left_type) > type_bytes(right_type)) {
                status = check_implicit_cast(c, right_type, left_type);
            }
            else {
                status = check_implicit_cast(c, left_type, right_type);
                return_left = false;
            }
            if (status == IMPLICIT_CAST_ERROR) {
                check_error(
                        expr->main_token,
                        "implicit cast error: `{t}` and `{t}`",
                        left_type,
                        right_type);
            }
            else if (return_left) {
                return left_type;
            } else {
                return right_type;
            }
        }
        else {
            check_error(
                    expr->main_token,
                    "operator `+` given `{t}` and `{t}`, but requires integer operands",
                    left_type,
                    right_type);
        }
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

Type* check_if_expr(CheckContext* c, Expr* expr, Type* cast) {
    // Get branch types
    Type* ifbr_type = check_if_branch(
            c, 
            expr->iff.ifbr,
            cast);
    Type** elseifbr_types = null;
    buf_loop(expr->iff.elseifbr, i) {
        buf_push(elseifbr_types, check_if_branch(
                    c,
                    expr->iff.elseifbr[i],
                    cast));
    }
    Type* elsebr_type = null;
    if (expr->iff.elsebr) {
        elsebr_type = check_if_branch(
                c, 
                expr->iff.elsebr,
                cast);
    }

    // Verify branch types
    // TODO: make it so that branch that pass in this "verify" stage are 
    // processed in "compare" stage, even if other branches fail
    if (!ifbr_type) {
        return null;
    }
    buf_loop(expr->iff.elseifbr, i) {
        if (expr->iff.elseifbr[i] && !elseifbr_types[i]) {
            return null;
        }
    }
    if (expr->iff.elsebr && !elsebr_type) {
        return null;
    }

    if (expr->iff.ifbr->body->block.value && !expr->iff.elsebr) {
        check_error(
                expr->main_token,
                "`else` is mandatory when value is used");
        // TODO: should we return??
    }

    // Compare branch types
    bool err = false;
    buf_loop(elseifbr_types, i) {
        if (check_implicit_cast(c, elseifbr_types[i], ifbr_type) == IMPLICIT_CAST_ERROR) {
            Token* errtok = null;
            if (expr->iff.elseifbr[i]->body->block.value) {
                errtok = expr->iff.elseifbr[i]->body->block.value->main_token;
            } 
            else {
                errtok = expr->iff.elseifbr[i]->body->block.rbrace;
            }

            check_error(
                    errtok,
                    "`if` returns `{t}`, but `else-if` returns `{t}`",
                    ifbr_type,
                    elseifbr_types[i]);
            err = true;
        }
    }

    if (expr->iff.elsebr) {
        if (check_implicit_cast(c, elsebr_type, ifbr_type) == IMPLICIT_CAST_ERROR) {
            Token* errtok = null;
            if (expr->iff.elsebr->body->block.value) {
                errtok = expr->iff.elsebr->body->block.value->main_token;
            } 
            else {
                errtok = expr->iff.elsebr->body->block.rbrace;
            }

            check_error(
                    errtok,
                    "`if` returns `{t}`, but `else` returns `{t}`",
                    ifbr_type,
                    elsebr_type);
            err = true;
        }
    }

    if (err) return null;
    return ifbr_type;
}

Type* check_if_branch(CheckContext* c, IfBranch* br, Type* cast) {
    if (br->cond) {
        Type* cond_type = check_expr(c, br->cond, null);
        if (cond_type && check_implicit_cast(
                    c, 
                    cond_type, 
                    builtin_type_placeholders.boolean) == IMPLICIT_CAST_ERROR) {
            check_error(
                    br->cond->main_token,
                    "branch condition should be `bool` but got `{t}`",
                    cond_type);
        }
    }
    return check_block_expr(c, br->body, cast);
}

Type* check_while_expr(CheckContext* c, Expr* expr, Type* cast) {
    Type* cond_type = check_expr(c, expr->whilelp.cond, null);
    if (cond_type && check_implicit_cast(
                c,
                cond_type,
                builtin_type_placeholders.boolean) == IMPLICIT_CAST_ERROR) {
        check_error(
                expr->whilelp.cond->main_token,
                "loop condition should be `bool` but got `{t}`",
                cond_type);
    }
    return check_block_expr(c, expr->whilelp.body, cast);
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
