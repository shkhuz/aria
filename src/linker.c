#include <linker.h>
#include <error_msg.h>
#include <builtin_types.h>
#include <arpch.h>

#define variable_redecl_error_msg "redeclaration of variable `%s`"

Linker linker_new(File* srcfile, Stmt** stmts) {
    Linker linker;
    linker.srcfile = srcfile;
    linker.stmts = stmts;
    linker.function_sym_tbl = null;
    linker.struct_sym_tbl = null;
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

static void add_struct(Linker* self, Stmt* stmt) {
    buf_loop(self->struct_sym_tbl, p) {
        Stmt* chk = self->struct_sym_tbl[p];
        if (is_tok_eq(
                    stmt->s.struct_decl.identifier,
                    chk->s.struct_decl.identifier)) {

            // TODO: see if the compiler has to
            // error on redefinition of struct
            // or compare the two structs and
            // see if they differ
            Token* token = stmt->s.struct_decl.identifier;
            error_token(
                    token,
                    "redefinition of struct `%s`",
                    token->lexeme
            );
            return;
        }
    }
    buf_push(self->struct_sym_tbl, stmt);
}

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
                        variable_redecl_error_msg,
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

static VariableScope is_variable_in_scope(Linker* self, Stmt* stmt) {
    VariableScope scope_in = VS_NO_SCOPE;
    bool first_iter = true;
    Scope* scope = self->current_scope;

    while (scope != null) {
        buf_loop(scope->variables, v) {
            Stmt* chk = scope->variables[v];
            if (is_tok_eq(
                        stmt->s.variable_decl.identifier,
                        chk->s.variable_decl.identifier)) {

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

static void check_and_add_to_scope(Linker* self, Stmt* stmt) {
    VariableScope scope = is_variable_in_scope(self, stmt);
    Token* identifier = stmt->s.variable_decl.identifier;
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
    buf_push(self->current_scope->variables, stmt);
}

static void check_data_type(Linker* self, DataType* dt, bool is_return_type) {
    if (is_dt_eq(dt, builtin_types.e.void_type) && !is_return_type) {
        error_data_type(
                dt,
                "invalid use of `void` type"
        );
        return;
    }

    bool found = false;
    buf_loop(self->struct_sym_tbl, s) {
        if (is_tok_eq(
                    dt->identifier,
                    self->struct_sym_tbl[s]->s.struct_decl.identifier)) {

            found = true;
            break;
        }
    }

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

static void check_function(Linker* self, Stmt* stmt) {
    change_scope(scope);

    Stmt** params = stmt->s.function.params;
    buf_loop(params, p) {
        check_data_type(self, params[p]->s.variable_decl.data_type, false);
        check_and_add_to_scope(self, params[p]);
    }

    check_data_type(self, stmt->s.function.return_type, true);

    revert_scope(scope);
}

static void check_stmt(Linker* self, Stmt* stmt) {
    switch (stmt->type) {
    case S_FUNCTION: check_function(self, stmt); break;
    }
}

Error linker_run(Linker* self) {
    buf_loop(self->stmts, s) {
        switch (self->stmts[s]->type) {
        case S_STRUCT: add_struct(self, self->stmts[s]); break;
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

