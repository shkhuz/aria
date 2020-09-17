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

            if (stmt->s.struct_decl.external &&
                chk->s.struct_decl.external) {
                if (str_intern(
                            stmt->s.struct_decl.identifier->srcfile->fpath
                    ) ==
                    str_intern(
                            chk->s.struct_decl.identifier->srcfile->fpath
                    )) {
                    return;
                }
            }

            Stmt** first_fields = chk->s.struct_decl.fields;
            Stmt** second_fields = stmt->s.struct_decl.fields;

            u64 first_flen = buf_len(first_fields);
            u64 second_flen = buf_len(second_fields);
            if (first_flen != second_flen) {
                error_token(
                        stmt->s.struct_decl.identifier,
                        "fields differ in redeclaration"
                );
                return;
            }

            bool error = false;
            buf_loop(first_fields, f) {
                if (!is_tok_eq(
                            first_fields[f]->s.variable_decl.identifier,
                            second_fields[f]->s.variable_decl.identifier)) {

                    error_token(
                            second_fields[f]->s.variable_decl.identifier,
                            "field name differs from previous declaration"
                    );
                    error = true;
                }

                if (!is_dt_eq(
                            first_fields[f]->s.variable_decl.data_type,
                            second_fields[f]->s.variable_decl.data_type)) {

                    error_data_type(
                            second_fields[f]->s.variable_decl.data_type,
                            "field type differs from previous declaration"
                    );
                    error = true;
                }
            }

            if (error) return;
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

static VariableScope is_variable_ref_in_scope(Linker* self, Expr* expr) {
    VariableScope scope_in = VS_NO_SCOPE;
    bool first_iter = true;
    Scope* scope = self->current_scope;

    while (scope != null) {
        buf_loop(scope->variables, v) {
            if (is_tok_eq(
                        expr->e.variable_ref.identifier,
                        scope->variables[v]->s.variable_decl.identifier)) {

                if (first_iter) scope_in = VS_CURRENT_SCOPE;
                else scope_in = VS_OUTER_SCOPE;
                expr->e.variable_ref.variable_ref = scope->variables[v];
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

static void check_expr(Linker* self, Expr* expr);

static void check_expr_binary(Linker* self, Expr* expr) {
    check_expr(self, expr->e.binary.left);
    check_expr(self, expr->e.binary.right);
}

static void check_expr_unary(Linker* self, Expr* expr) {
    check_expr(self, expr->e.unary.right);
}

static void check_expr_function_call(Linker* self, Expr* expr) {
    assert(expr->e.function_call.left->type == E_VARIABLE_REF);
    buf_loop(self->function_sym_tbl, f) {
        if (is_tok_eq(
                    expr->e.function_call.left->e.variable_ref.identifier,
                    self->function_sym_tbl[f]->s.function.identifier)) {
            expr->e.function_call.called = self->function_sym_tbl[f];
            break;
        }
    }

    if (!expr->e.function_call.called) {
        error_expr(
                expr->e.function_call.left,
                "undefined function `%s`",
                expr->e.function_call.left->e.variable_ref.identifier->lexeme
        );
        return;
    }

    assert(expr->e.function_call.called);
    u64 arg_len = buf_len(expr->e.function_call.args);
    u64 param_len = buf_len(expr->e.function_call.called->s.function.params);
    if (arg_len != param_len) {
        error_expr(
                expr->e.function_call.left,
                "function expects %lu argument(s), but given %lu",
                param_len,
                arg_len
        );
    }

    buf_loop(expr->e.function_call.args, a) {
        check_expr(self, expr->e.function_call.args[a]);
    }
}

static void check_expr_variable_ref(Linker* self, Expr* expr) {
    VariableScope scope = is_variable_ref_in_scope(self, expr);
    if (scope == VS_NO_SCOPE) {
        error_expr(
                expr,
                "undefined variable `%s`",
                expr->e.variable_ref.identifier->lexeme
        );
        return;
    }
}

static void check_expr(Linker* self, Expr* expr) {
    switch (expr->type) {
    case E_BINARY: check_expr_binary(self, expr); break;
    case E_UNARY: check_expr_unary(self, expr); break;
    case E_FUNCTION_CALL: check_expr_function_call(self, expr); break;
    case E_VARIABLE_REF: check_expr_variable_ref(self, expr); break;
    case E_INTEGER:
    case E_STRING:
    case E_CHAR:
    case E_NONE: break;
    default: assert(0); break;
    }
}

static void check_stmt(Linker* self, Stmt* stmt);

static void check_stmt_struct(Linker* self, Stmt* stmt) {
    change_scope(scope);

    Stmt** fields = stmt->s.struct_decl.fields;
    buf_loop(fields, f) {
        check_data_type(self, fields[f]->s.variable_decl.data_type, false);

        VariableScope scope_in = is_variable_in_scope(self, fields[f]);
        if (scope_in == VS_CURRENT_SCOPE) {
            Token* identifier = fields[f]->s.variable_decl.identifier;
            error_token(
                    identifier,
                    "redeclaration of field `%s`",
                    identifier->lexeme
            );
            continue;
        }

        buf_push(scope->variables, fields[f]);
    }

    revert_scope(scope);
}

static void check_stmt_function(Linker* self, Stmt* stmt) {
    change_scope(scope);

    Stmt** params = stmt->s.function.params;
    buf_loop(params, p) {
        // TODO: see if adding param first to the
        // symbol table generates fewer errors
        check_data_type(self, params[p]->s.variable_decl.data_type, false);
        check_and_add_to_scope(self, params[p]);
    }

    check_data_type(self, stmt->s.function.return_type, true);

    if (!stmt->s.function.decl) {
        check_stmt(self, stmt->s.function.body);
    }

    revert_scope(scope);
}

static void check_stmt_variable_decl(Linker* self, Stmt* stmt) {
    if (stmt->s.variable_decl.data_type && !stmt->s.variable_decl.external) {
        check_data_type(self, stmt->s.variable_decl.data_type, false);
    }

    if (!stmt->s.variable_decl.external) {
        if (stmt->s.variable_decl.initializer) {
            check_expr(self, stmt->s.variable_decl.initializer);
        }
    }

    if (!stmt->s.variable_decl.global) {
        check_and_add_to_scope(self, stmt);
    }
}

static void check_stmt_block(Linker* self, Stmt* stmt) {
    change_scope(scope);
    buf_loop(stmt->s.block, s) {
        check_stmt(self, stmt->s.block[s]);
    }
    revert_scope(scope);
}

static void check_stmt_expr(Linker* self, Stmt* stmt) {
    check_expr(self, stmt->s.expr);
}

static void check_stmt_return(Linker* self, Stmt* stmt) {
    check_expr(self, stmt->s.ret);
}

static void check_stmt(Linker* self, Stmt* stmt) {
    switch (stmt->type) {
    case S_STRUCT: check_stmt_struct(self, stmt); break;
    case S_FUNCTION: check_stmt_function(self, stmt); break;
    case S_VARIABLE_DECL: check_stmt_variable_decl(self, stmt); break;
    case S_BLOCK: check_stmt_block(self, stmt); break;
    case S_EXPR: check_stmt_expr(self, stmt); break;
    case S_RETURN: check_stmt_return(self, stmt); break;
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

