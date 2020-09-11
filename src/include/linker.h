#ifndef _LINKER_H
#define _LINKER_H

#include <stmt.h>
#include <arpch.h>
#include <error_value.h>

typedef struct Scope Scope;
struct Scope {
	Scope* parent_scope;
	Stmt** variables;
};

typedef enum {
	VS_CURRENT_SCOPE,
	VS_OUTER_SCOPE,
	VS_NO_SCOPE,
} VariableScope;

typedef struct {
    File* srcfile;
    Stmt** stmts;
    Stmt** function_sym_tbl;
    Error error_state;
} Linker;

Linker linker_new(File* srcfile, Stmt** stmts);
Error linker_run(Linker* self);

#endif /* _LINKER_H */

