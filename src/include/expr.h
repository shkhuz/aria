#ifndef _EXPR_H
#define _EXPR_H

#include <token.h>

typedef enum {
    E_BINARY,
    E_UNARY,
    E_INTEGER,
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

struct Expr {
    ExprType type;
    union e {
        ExprBinary binary;
        ExprUnary unary;
        Token* integer;
    } e;
};

Expr expr_new(void);
Expr* expr_new_alloc(void);

#endif /* _EXPR_H */
