#ifndef _STMT_H
#define _STMT_H

#include <expr.h>

typedef enum {
    S_FUNCTION_DEF,
    S_EXPR,
    S_NONE,
} StmtType;

typedef struct Stmt Stmt;

typedef struct {
    Token* identifier;
    Stmt** body;
} FunctionDef;

struct Stmt {
    StmtType type;
    union s {
        FunctionDef function_def;
        Expr* expr;
    } s;
};

Stmt stmt_new(void);
Stmt* stmt_new_alloc(void);

#endif
