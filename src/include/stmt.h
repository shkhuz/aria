#ifndef _STMT_H
#define _STMT_H

#include <expr.h>
#include <data_type.h>

typedef enum {
    S_FUNCTION,
    S_VARIABLE_DECL,
    S_EXPR,
    S_BLOCK,
    S_NONE,
} StmtType;

typedef struct Stmt Stmt;

typedef struct {
    Token* identifier;
    DataType* data_type;
} Param;

typedef struct {
    Token* identifier;
    Param** params;
    DataType* return_type;
    Stmt* body;
    bool decl;
} Function;

typedef struct {
    Token* identifier;
    DataType* data_type;
    Expr* initializer;
} VariableDecl;

struct Stmt {
    StmtType type;
    union s {
        Function function;
        VariableDecl variable_decl;
        Expr* expr;
        Stmt** block;
    } s;
};

Param param_new(Token* identifier, DataType* data_type);
Param* param_new_alloc(Token* identifier, DataType* data_type);
Stmt stmt_new(void);
Stmt* stmt_new_alloc(void);

#endif /* _STMT_H */
