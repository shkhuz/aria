#include <stmt.h>
#include <arpch.h>

Stmt stmt_new(void) {
    Stmt stmt;
    stmt.type = S_NONE;
    return stmt;
}

Stmt* stmt_new_alloc(void) {
    Stmt* stmt = malloc(sizeof(Stmt));
    *stmt = stmt_new();
    return stmt;
}
