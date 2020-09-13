#include <linker.h>
#include <error_msg.h>
#include <arpch.h>

Linker linker_new(File* srcfile, Stmt** stmts) {
    Linker linker;
    linker.srcfile = srcfile;
    linker.stmts = stmts;
    linker.function_sym_tbl = null;
    linker.global_scope = scope_new(null);
    linker.current_scope = linker.global_scope;
    linker.error_state = ERROR_SUCCESS;
    return linker;
}

#define change_scope(name) \
    Scope* name = scope_new(self->current_scope); \
    self->current_scope = name;

#define revert_scope(name) \
    self->current_scope = name->parent_scope;

static void add_function(Linker* self, Stmt* stmt) {
    buf_loop(self->function_sym_tbl, p) {
        Stmt* chk = self->function_sym_tbl[p];
        if (is_tok_eq(
                    stmt->s.function.identifier,
                    chk->s.function.identifier)) {

            if (!chk->s.function.decl && !stmt->s.function.decl) {
                Token* token = stmt->s.function.identifier;
                error_token(
                        token,
                        "redefinition of function: `%s`",
                        token->lexeme
                );
                return;
            }
            else {
                Stmt** first_params = chk->s.function.params;
                Stmt** second_params = stmt->s.function.params;

                u64 first_arity = buf_len(first_params);
                u64 second_arity = buf_len(second_params);

                if (second_arity != first_arity) {
                    Token* token = stmt->s.function.identifier;
                    error_token(
                            token,
                            "conflicting parameter arity [%lu %s %lu]",
                            second_arity,
                            (second_arity > first_arity ? ">" :
                             (second_arity < first_arity ? "<" : "=")),
                            first_arity
                    );
                    return;
                }

                bool error = false;
                buf_loop(second_params, i) {
                    if (!is_tok_eq(
                                second_params[i]->s.variable_decl.identifier,
                                first_params[i]->s.variable_decl.identifier)) {

                        Token* token =
                            second_params[i]->s.variable_decl.identifier;
                        error_token(
                                token,
                                "parameter name does not match previous declaration"
                        );
                        error = true;
                    }

                    if (!is_dt_eq(
                                second_params[i]->s.variable_decl.data_type,
                                first_params[i]->s.variable_decl.data_type)) {

                        DataType* dt =
                            second_params[i]->s.variable_decl.data_type;
                        assert(!dt->compiler_generated);
                        error_data_type(
                                dt,
                                "parameter type does not match previous declaration"
                        );
                        error = true;
                    }
                }

                if (!is_dt_eq(
                            stmt->s.function.return_type,
                            chk->s.function.return_type)) {

                    DataType* dt = stmt->s.function.return_type;
                    const char* error_msg =
                        "return type does not match previous declaration";

                    if (dt->compiler_generated) {
                        error_token(
                                stmt->s.function.identifier,
                                error_msg
                        );
                    }
                    else { error_data_type(dt, error_msg); }
                    error = true;
                }

                if (error) return;
            }
        }
    }
    buf_push(self->function_sym_tbl, stmt);
}

static void add_variable_decl(Linker* self, Stmt* stmt) {
    bool error = false;

    Expr* initializer = stmt->s.variable_decl.initializer;
    if (initializer) {
        error_expr(
                initializer,
                "global-variable initializer is not permitted"
        );
        error = true;
    }

    buf_loop(self->global_scope->variables, v) {
        Stmt* chk = self->global_scope->variables[v];
        if (is_tok_eq(
                    stmt->s.variable_decl.identifier,
                    chk->s.variable_decl.identifier)) {

            if (!stmt->s.variable_decl.external &&
                !chk->s.variable_decl.external) {
                Token* token = stmt->s.variable_decl.identifier;
                error_token(
                        token,
                        "redeclaration of variable `%s`",
                        token->lexeme
                );
                error = true;
            }
            else {
                DataType* dt = stmt->s.variable_decl.data_type;
                if (!is_dt_eq_null(
                            dt,
                            chk->s.variable_decl.data_type)) {

                    const char* error_msg = "variable type differs from previous declaration";
                    if (dt) {
                        error_data_type(
                                dt,
                                error_msg
                        );
                        error = true;
                    }
                    else {
                        error_token(
                                stmt->s.variable_decl.identifier,
                                error_msg
                        );
                        error = true;
                    }
                }
            }

            if (error) return;
        }
    }
    if (error) return;

    buf_push(self->global_scope->variables, stmt);
}

static void check_stmt(Linker* self, Stmt* stmt) {
    /* switch (self->stmts[s]->type) { */
    /* case S_FUNCTION: check_function(self, stmt); break; */
    /* } */
}

Error linker_run(Linker* self) {
    buf_loop(self->stmts, s) {
        switch (self->stmts[s]->type) {
        case S_FUNCTION: add_function(self, self->stmts[s]); break;
        case S_VARIABLE_DECL: add_variable_decl(self, self->stmts[s]); break;
        default: break;
        }
    }

    buf_loop(self->stmts, s) {
        check_stmt(self, self->stmts[s]);
    }

    return self->error_state;
}

