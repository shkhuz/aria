#ifndef _EXPR_H
#define _EXPR_H

#include "token.h"

typedef enum {
    E_INTEGER,
    E_BLOCK,
    E_NONE,
} ExprType;

typedef struct Expr Expr;
struct Stmt;

typedef struct {
    struct Stmt** stmts;
    Expr* ret;
} ExprBlock;

struct Expr {
    ExprType type;
    Token* head;
    Token* tail;
    union {
        ExprBlock block;
        Token* integer;
    };
};

Expr expr_new(void);
Expr* expr_new_alloc(void);

#endif /* _EXPR_H */
