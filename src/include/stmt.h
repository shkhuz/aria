#ifndef _STMT_H
#define _STMT_H

#include <expr.h>

typedef enum {
    S_EXPR,
    S_NONE,
} StmtType;

typedef struct {
    StmtType type;
    union s {
        Expr* expr;
    } s;
} Stmt;

Stmt stmt_new(void);
Stmt* stmt_new_alloc(void);

#endif
