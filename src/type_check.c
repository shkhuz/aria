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
    chk((left_type = typeck_expr(self, check->assign.left),
        right_type = typeck_expr(self, check->assign.right))
    );
    if (!is_dt_eq(left_type, right_type)) {
        error_expr(
                check->assign.right,
                "conflicting types for assignment operator"
        );
        return null;
    }
    return left_type;
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
    case E_ASSIGN: return typeck_assign_expr(self, check); break;
    case E_INTEGER: return builtin_types.e.u64_type; break;
    case E_VARIABLE_REF: return typeck_variable_ref_expr(self, check); break;
    case E_BLOCK: return typeck_block_expr(self, check); break;
    default: assert(0); break;
    }
}

static void typeck_function(TypeChecker* self, Stmt* check) {
    DataType* block_type = null;
    chkv(block_type = typeck_expr(self, check->function.block));
    if (block_type == builtin_types.e.void_type) return;

    if (!is_dt_eq(block_type, check->function.return_type)) {
        error_token(
                check->function.identifier,
                "return type conflicts from last expression returned"
        );
    }
}

static void typeck_variable_decl(TypeChecker* self, Stmt* check) {
    if (!check->variable_decl.data_type) {
        check->variable_decl.data_type =
            typeck_expr(self, check->variable_decl.initializer);
        return;
    }

    if (check->variable_decl.initializer && check->variable_decl.data_type) {
        // TODO: check if this error recovery system works
        DataType* assign_type = null;
        chkv(assign_type =
                typeck_expr(self, check->variable_decl.initializer));
        if (!is_dt_eq(check->variable_decl.data_type, assign_type)) {
            error_expr(
                    check->variable_decl.initializer,
                    "initializer type conflicts from annotated type"
            );
        }
    }
}

static void typeck_return_stmt(TypeChecker* self, Stmt* check) {
}

static void typeck_expr_stmt(TypeChecker* self, Stmt* check) {
    DataType* block_type = null;
    chkv(block_type = typeck_expr(self, check->expr));
    if (check->expr->type == E_BLOCK) {
        if (!is_dt_eq(block_type, builtin_types.e.void_type)) {
            error_expr(
                    check->expr->block.ret,
                    "freestanding block ought to return `void`"
            );
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
    self->error_count = 0;
    self->error_state = false;

    buf_loop(self->ast->stmts, s) {
        typeck_stmt(self, self->ast->stmts[s]);
    }

    return self->error_state;
}
