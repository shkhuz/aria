#ifndef _EXPR_H
#define _EXPR_H

#include "token.h"

typedef enum {
    E_ASSIGN,
    E_INTEGER,
    E_VARIABLE_REF,
    E_BLOCK,
    E_NONE,
} ExprType;

typedef struct Expr Expr;
struct Stmt;

typedef struct {
    Expr* left;
    Expr* right;
} ExprAssign;

typedef struct {
    Token* identifier;
    struct Stmt* declaration;
} ExprVariableRef;

typedef struct {
    struct Stmt** stmts;
    Expr* ret;
} ExprBlock;

struct Expr {
    ExprType type;
    Token* head;
    Token* tail;
    union {
        ExprAssign assign;
        ExprVariableRef variable_ref;
        ExprBlock block;
        Token* integer;
    };
};

Expr expr_new(void);
Expr* expr_new_alloc(void);

#endif /* _EXPR_H */
