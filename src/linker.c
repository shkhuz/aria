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
        if (str_intern(stmt->s.function_def.identifier->lexeme) ==
            str_intern(chk->s.function_def.identifier->lexeme)) {
            Token* token = stmt->s.function_def.identifier;
            error_token(
                    token,
                    "redefinition of function: `%s`",
                    token->lexeme
            );
            return;
        }
    }
    buf_push(self->function_sym_tbl, stmt);
}

Error linker_run(Linker* self) {
    buf_loop(self->stmts, s) {
        if (self->stmts[s]->type == S_FUNCTION_DEF) {
            add_function(self, self->stmts[s]);
        }
    }
    return self->error_state;
}
