#ifndef _EXPR_H
#define _EXPR_H

#include <token.h>

typedef enum {
    E_BINARY,
    E_UNARY,
    E_VARIABLE_REF,
    E_INTEGER,
    E_STRING,
    E_CHAR,
    E_NONE,
} ExprType;

typedef struct Expr Expr;

typedef struct {
    Expr* left;
    Expr* right;
    Token* op;
} ExprBinary;

typedef struct {
    Token* op;
    Expr* right;
} ExprUnary;

typedef struct {
    Token* identifier;
    struct Stmt* variable_ref;
} ExprVariableRef;

struct Expr {
    ExprType type;
    Token* head;
    Token* tail;
    union e {
        ExprBinary binary;
        ExprUnary unary;
        ExprVariableRef variable_ref;
        Token* integer;
        Token* str;
        Token* chr;
    } e;
};

Expr expr_new(void);
Expr* expr_new_alloc(void);

#endif /* _EXPR_H */
