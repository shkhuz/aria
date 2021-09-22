#ifndef RESOLVE_H
#define RESOLVE_H

#include "core.h"
#include "aria.h"

typedef struct Scope {
    struct Scope* parent;
    Stmt** stmts;
} Scope;

typedef struct {
    Srcfile* srcfile;
    Scope* global_scope;
    Scope* current_scope;
    Stmt* current_function;
    bool error;
} ResolveContext;

void resolve(ResolveContext* r);

#endif
