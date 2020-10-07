#ifndef _EXPR_H
#define _EXPR_H

#include "token.h"

typedef enum {
    E_BINARY,
    E_UNARY,
    E_FUNC_CALL,
    E_INTEGER,
    E_VARIABLE_REF,
    E_STRING,
    E_BLOCK,
    E_NONE,
} ExprType;

typedef struct Expr Expr;
struct Stmt;

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
    Expr* left;
    Expr** args;
    struct Stmt* callee;
} ExprFuncCall;

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
        ExprBinary binary;
        ExprUnary unary;
        ExprFuncCall func_call;
        ExprVariableRef variable_ref;
        ExprBlock block;
        Token* integer;
        Token* string;
    };
};

Expr expr_new(void);
Expr* expr_new_alloc(void);
Expr* expr_variable_ref_from_token(Token* identifier);
Expr* expr_variable_ref_from_string(const char* identifier);

#endif /* _EXPR_H */
