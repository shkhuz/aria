#include <linker.h>
#include <error_msg.h>
#include <arpch.h>

Linker linker_new(File* srcfile, Stmt** stmts) {
    Linker linker;
    linker.srcfile = srcfile;
    linker.stmts = stmts;
    linker.function_sym_tbl = null;
    linker.error_state = ERROR_SUCCESS;
    return linker;
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
                Param** first_params = chk->s.function.params;
                Param** second_params = stmt->s.function.params;

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

                buf_loop(second_params, i) {
                    if (!is_tok_eq(
                                second_params[i]->identifier,
                                first_params[i]->identifier)) {

                        Token* token = second_params[i]->identifier;
                        error_token(
                                token,
                                "parameter name does not match previous declaration"
                        );
                    }
                }
            }
        }
    }
    buf_push(self->function_sym_tbl, stmt);
}

Error linker_run(Linker* self) {
    buf_loop(self->stmts, s) {
        switch (self->stmts[s]->type) {
        case S_FUNCTION:
            add_function(self, self->stmts[s]); break;
        }
    }
    return self->error_state;
}
