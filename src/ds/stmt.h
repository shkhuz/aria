#ifndef _STMT_H
#define _STMT_H

#include "expr.h"
#include "data_type.h"
#include "token.h"

typedef enum {
    S_FUNCTION,
    S_VARIABLE_DECL,
    S_RETURN,
    S_EXPR,
    S_NONE,
} StmtType;

typedef struct Stmt Stmt;

typedef struct {
    Token* identifier;
    Stmt** params;
    Stmt** variable_decls;
    DataType* return_type;
    Expr* block;
    bool decl;
    bool pub;
} Function;

typedef struct {
    Token* identifier;
    DataType* data_type;
    Expr* initializer;
    bool global;
    bool external;
    /* offset from frame pointer */
    u64 offset;
} VariableDecl;

typedef struct {
    Token* return_keyword;
    Expr* expr;
} ExprReturn;

struct Stmt {
    StmtType type;
    union {
        Function function;
        VariableDecl variable_decl;
        Expr* expr;
        ExprReturn ret;
    };
};

Stmt stmt_new(void);
Stmt* stmt_new_alloc(void);

#endif /* _STMT_H */

