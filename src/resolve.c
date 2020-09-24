#include "arpch.h"
#include "util/util.h"
#include "error_msg.h"
#include "ds/ds.h"
#include "aria.h"

static void insert_function_into_sym_tbl(Resolver* self, Stmt* check) {
    buf_loop(self->func_sym_tbl, f) {
        Stmt* check_against = self->func_sym_tbl[f];
        if (is_tok_eq(
                    check->function.identifier,
                    check_against->function.identifier)) {
            error_token(
                    check->function.identifier,
                    "redefinition of function: `%s`",
                    check->function.identifier->lexeme
            );
            return;
        }
    }
    buf_push(self->func_sym_tbl, check);
}

bool resolve_ast(Resolver* self, Ast* ast) {
    self->ast = ast;
    self->func_sym_tbl = null;
    self->error_state = false;

    buf_loop(self->ast->stmts, s) {
        Stmt* current_stmt = self->ast->stmts[s];
        switch (current_stmt->type) {
        case S_FUNCTION:
            insert_function_into_sym_tbl(self, current_stmt); break;
        default: assert(0); break;
        }
    }

    return self->error_state;
}

