#ifndef _LINKER_H
#define _LINKER_H

#include <stmt.h>
#include <arpch.h>
#include <scope.h>
#include <error_value.h>

typedef struct {
    File* srcfile;
    Stmt** stmts;
    Stmt** function_sym_tbl;

    Scope* global_scope;
    Scope* current_scope;
    Error error_state;
} Linker;

Linker linker_new(File* srcfile, Stmt** stmts);
Error linker_run(Linker* self);

#endif /* _LINKER_H */

