#include "arpch.h"
#include "util/util.h"
#include "error_recover.h"
#include "error_msg.h"
#include "ds/ds.h"
#include "aria.h"

static void typeck_stmt(TypeChecker* self, Stmt* check);
static DataType* typeck_expr(TypeChecker* self, Expr* check);

static DataType* typeck_assign_expr(TypeChecker* self, Expr* check) {
    DataType* left_type = null;
    DataType* right_type = null;
    chk((left_type = typeck_expr(self, check->binary.left),
        right_type = typeck_expr(self, check->binary.right))
    );
    if (!is_dt_eq(left_type, right_type)) {
        error_expr(
                check->binary.right,
                "conflicting types for assignment operator"
        );
        error_info_expect_type(left_type);
        error_info_got_type(right_type);
        return null;
    }
    return left_type;
}

static DataType* typeck_add_sub_expr(TypeChecker* self, Expr* check) {
    DataType* left_type = null;
    DataType* right_type = null;
    chk((left_type = typeck_expr(self, check->binary.left),
        right_type = typeck_expr(self, check->binary.right))
    );

    bool pre_error = false;
    const char* int_operand_err_msg =
        "`%s` operator requires integer operand";

    if (!is_dt_integer(left_type)) {
        error_expr(
                check->binary.left,
                int_operand_err_msg,
                check->binary.op->lexeme
        );
        pre_error = true;
    }

    if (!is_dt_integer(right_type)) {
        error_expr(
                check->binary.right,
                int_operand_err_msg,
                check->binary.op->lexeme
        );
        pre_error = true;
    }

    if (!pre_error) {
        if (!is_dt_eq(left_type, right_type)) {
            error_token(
                    check->binary.op,
                    "`%s` operator requires similar types",
                    check->binary.op->lexeme
            );
            error_info_expect_type(left_type);
            error_info_got_type(right_type);
            pre_error = true;
        }
    }

    if (pre_error) return null;
    return left_type;
}

static DataType* typeck_binary_expr(TypeChecker* self, Expr* check) {
    switch (check->binary.op->type) {
    case T_PLUS:
    case T_MINUS: return typeck_add_sub_expr(self, check); break;
    case T_EQUAL: return typeck_assign_expr(self, check); break;
    default: assert(0); break;
    }
}

static DataType* typeck_variable_ref_expr(TypeChecker* self, Expr* check) {
    if (check->variable_ref.declaration->variable_decl.data_type) {
        return check->variable_ref.declaration->variable_decl.data_type;
    }
    else assert(0);
}

static DataType* typeck_block_expr(TypeChecker* self, Expr* check) {
    buf_loop(check->block.stmts, s) {
        typeck_stmt(self, check->block.stmts[s]);
    }

    if (check->block.ret) {
        return typeck_expr(self, check->block.ret);
    }
    return builtin_types.e.void_type;
}

static DataType* typeck_expr(TypeChecker* self, Expr* check) {
    switch (check->type) {
    case E_BINARY: return typeck_binary_expr(self, check); break;
    case E_INTEGER: return builtin_types.e.u64_type; break;
    case E_VARIABLE_REF: return typeck_variable_ref_expr(self, check); break;
    case E_BLOCK: return typeck_block_expr(self, check); break;
    default: assert(0); break;
    }
}

static void typeck_function(TypeChecker* self, Stmt* check) {
    self->enclosed_function = check;
    DataType* block_type = null;
    es; block_type = typeck_expr(self, check->function.block); self->enclosed_function = null; er;
    if (block_type == builtin_types.e.void_type) return;

    if (!is_dt_eq(block_type, check->function.return_type)) {
        const char* err_msg =
            "return type conflicts from last expression returned";
        if (check->function.block->block.ret) {
            error_expr(
                    check->function.block->block.ret,
                    err_msg
            );
        }
        else {
            error_token(
                    check->function.identifier,
                    err_msg
            );
        }
        error_info_expect_type(check->function.return_type);
        error_info_got_type(block_type);
    }
}

static void typeck_variable_decl(TypeChecker* self, Stmt* check) {
    if (!check->variable_decl.data_type) {
        check->variable_decl.data_type =
            typeck_expr(self, check->variable_decl.initializer);
    } else if (check->variable_decl.initializer &&
               check->variable_decl.data_type) {
        // TODO: check if this error recovery system works
        DataType* assign_type = null;
        chkv(assign_type =
                typeck_expr(self, check->variable_decl.initializer));
        if (!is_dt_eq(check->variable_decl.data_type, assign_type)) {
            error_expr(
                    check->variable_decl.initializer,
                    "initializer type conflicts from annotated type"
            );
            error_info_expect_type(check->variable_decl.data_type);
            error_info_got_type(assign_type);
        }
    }

    if (self->enclosed_function) {
        buf_push(self->enclosed_function->function.variable_decls, check);
    }
}

static void typeck_return_stmt(TypeChecker* self, Stmt* check) {
    DataType* return_type = null;
    if (check->ret.expr) {
        chkv(return_type = typeck_expr(self, check->ret.expr));
    } else {
        return_type = builtin_types.e.void_type;
    }

    if (!is_dt_eq(self->enclosed_function->function.return_type, return_type)) {
        const char* err_msg =
            "expression type conflicts with return type";
        if (check->ret.expr) {
            error_expr(
                    check->ret.expr,
                    err_msg
            );
        } else {
            error_token(
                    check->ret.return_keyword,
                    err_msg
            );
        }
        error_info_expect_type(self->enclosed_function->function.return_type);
        error_info_got_type(return_type);
    }
}

static void typeck_expr_stmt(TypeChecker* self, Stmt* check) {
    DataType* block_type = null;
    chkv(block_type = typeck_expr(self, check->expr));
    if (check->expr->type == E_BLOCK) {
        if (!is_dt_eq(builtin_types.e.void_type, block_type)) {
            error_expr(
                    check->expr->block.ret,
                    "freestanding block ought to return `void`"
            );
            error_info_expect_type(builtin_types.e.void_type);
            error_info_got_type(block_type);
        }
    }
}

static void typeck_stmt(TypeChecker* self, Stmt* check) {
    switch (check->type) {
    case S_FUNCTION: typeck_function(self, check); break;
    case S_VARIABLE_DECL: typeck_variable_decl(self, check); break;
    case S_RETURN: typeck_return_stmt(self, check); break;
    case S_EXPR: typeck_expr_stmt(self, check); break;
    /* default: assert(0); break; */
    }
}

bool type_check_ast(TypeChecker* self, Ast* ast) {
    self->ast = ast;
    self->enclosed_function = null;
    self->error_count = 0;
    self->error_state = false;

    buf_loop(self->ast->stmts, s) {
        typeck_stmt(self, self->ast->stmts[s]);
    }

    return self->error_state;
}

