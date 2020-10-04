#include "arpch.h"
#include "util/util.h"
#include "error_msg.h"
#include "ds/ds.h"
#include "aria.h"

#define change_scope(name) \
    Scope* name = scope_new(self->current_scope); \
    self->current_scope = name;

#define revert_scope(name) \
    self->current_scope = name->parent_scope;

static void resolve_stmt(Resolver* self, Stmt* check);
static void resolve_expr(Resolver* self, Expr* check);

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

#define variable_redecl_error_msg "redeclaration of variable `%s`"

static void insert_variable_into_sym_tbl(Resolver* self, Stmt* check) {
    Expr* initializer = check->variable_decl.initializer;
    if (initializer) {
        error_expr(
                initializer,
                "global-variable initializer is not permitted"
        );
    }

    buf_loop(self->global_scope->variables, v) {
        Stmt* check_against = self->global_scope->variables[v];
        if (is_tok_eq(
                    check->variable_decl.identifier,
                    check_against->variable_decl.identifier)) {
            error_token(
                    check->variable_decl.identifier,
                    variable_redecl_error_msg,
                    check->variable_decl.identifier->lexeme
            );
            return;
        }
    }
    buf_push(self->global_scope->variables, check);
}

static VariableScope is_variable_in_scope(Resolver* self, Stmt* check) {
    VariableScope scope_in = VS_NO_SCOPE;
    bool first_iter = true;
    Scope* scope = self->current_scope;

    while (scope != null) {
        buf_loop(scope->variables, v) {
            Stmt* check_against = scope->variables[v];
            if (is_tok_eq(
                        check->variable_decl.identifier,
                        check_against->variable_decl.identifier)) {

                if (first_iter) scope_in = VS_CURRENT_SCOPE;
                else scope_in = VS_OUTER_SCOPE;
                break;
            }
        }

        if (scope_in != VS_NO_SCOPE) break;
        scope = scope->parent_scope;
        first_iter = false;
    }

    return scope_in;
}

static VariableScope is_variable_ref_in_scope(Resolver* self, Expr* expr) {
    VariableScope scope_in = VS_NO_SCOPE;
    bool first_iter = true;
    Scope* scope = self->current_scope;

    while (scope != null) {
        buf_loop(scope->variables, v) {
            if (is_tok_eq(
                        expr->variable_ref.identifier,
                        scope->variables[v]->variable_decl.identifier)) {

                if (first_iter) scope_in = VS_CURRENT_SCOPE;
                else scope_in = VS_OUTER_SCOPE;
                expr->variable_ref.declaration = scope->variables[v];
                break;
            }
        }

        if (scope_in != VS_NO_SCOPE) break;
        scope = scope->parent_scope;
        first_iter = false;
    }

    return scope_in;
}

static void check_and_add_to_scope(Resolver* self, Stmt* check) {
    VariableScope scope = is_variable_in_scope(self, check);
    Token* identifier = check->variable_decl.identifier;
    if (scope == VS_CURRENT_SCOPE) {
        error_token(
                identifier,
                variable_redecl_error_msg,
                identifier->lexeme
        );
        return;
    }
    else if (scope == VS_OUTER_SCOPE) {
        error_token(
                identifier,
                "variable shadows another variable"
        );
        return;
    }
    buf_push(self->current_scope->variables, check);
}

static void resolve_data_type(Resolver* self, DataType* dt) {
    bool found = false;
    if (!found) {
        for (u64 t = 0; t < BUILTIN_TYPES_LEN; t++) {
            if (is_tok_eq(
                        builtin_types.l[t]->identifier,
                        dt->identifier)) {
                found = true;
                break;
            }
        }
    }

    if (!found) {
        error_data_type(
                dt,
                "undefined type"
        );
        return;
    }
}

static void resolve_binary_expr(Resolver* self, Expr* check) {
    resolve_expr(self, check->binary.left);
    resolve_expr(self, check->binary.right);
}

static void resolve_block_expr(Resolver* self, Expr* check) {
    change_scope(scope);
    buf_loop(check->block.stmts, s) {
        resolve_stmt(self, check->block.stmts[s]);
    }
    if (check->block.ret) {
        resolve_expr(self, check->block.ret);
    }
    revert_scope(scope);
}

static void resolve_variable_ref_expr(Resolver* self, Expr* check) {
	VariableScope scope_in_found = is_variable_ref_in_scope(self, check);
	if (scope_in_found == VS_NO_SCOPE) {
        error_expr(
                check,
                "undefined variable: `%s`",
                check->variable_ref.identifier->lexeme
        );
	}
}

static void resolve_expr(Resolver* self, Expr* check) {
    switch (check->type) {
    case E_BINARY: resolve_binary_expr(self, check); break;
    case E_BLOCK: resolve_block_expr(self, check); break;
    case E_VARIABLE_REF: resolve_variable_ref_expr(self, check); break;
    }
}

static void resolve_function(Resolver* self, Stmt* check) {
    change_scope(scope);

    Stmt** params = check->function.params;
    buf_loop(params, p) {
        // TODO: see if adding param first to the
        // symbol table generates fewer errors
        resolve_data_type(self, params[p]->variable_decl.data_type);
        check_and_add_to_scope(self, params[p]);
    }

    resolve_data_type(self, check->function.return_type);

    resolve_expr(self, check->function.block);

    revert_scope(scope);
}

static void resolve_variable_decl(Resolver* self, Stmt* check) {
    if (check->variable_decl.data_type) {
        resolve_data_type(self, check->variable_decl.data_type);
    }

    if (check->variable_decl.initializer) {
        resolve_expr(self, check->variable_decl.initializer);
    }

    if (!check->variable_decl.global) {
        check_and_add_to_scope(self, check);
    }
}

static void resolve_return_stmt(Resolver* self, Stmt* check) {
    if (check->ret.expr) {
        resolve_expr(self, check->ret.expr);
    }
}

static void resolve_stmt(Resolver* self, Stmt* check) {
    switch (check->type) {
    case S_FUNCTION: resolve_function(self, check); break;
    case S_VARIABLE_DECL: resolve_variable_decl(self, check); break;
    case S_RETURN: resolve_return_stmt(self, check); break;
    case S_EXPR: resolve_expr(self, check->expr); break;
    }
}

bool resolve_ast(Resolver* self, Ast* ast) {
    self->ast = ast;
    self->func_sym_tbl = null;
    self->global_scope = scope_new(null);
    self->current_scope = self->global_scope;
    self->error_state = false;
    self->error_count = 0;

    buf_loop(self->ast->stmts, s) {
        Stmt* current_stmt = self->ast->stmts[s];
        switch (current_stmt->type) {
        case S_FUNCTION:
            insert_function_into_sym_tbl(self, current_stmt); break;
        case S_VARIABLE_DECL:
            insert_variable_into_sym_tbl(self, current_stmt); break;
        default: assert(0); break;
        }
    }

    buf_loop(self->ast->stmts, s) {
        Stmt* current_stmt = self->ast->stmts[s];
        resolve_stmt(self, current_stmt);
    }

    return self->error_state;
}

