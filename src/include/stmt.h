#ifndef _STMT_H
#define _STMT_H

#include <expr.h>
#include <data_type.h>

typedef enum {
    S_STRUCT,
    S_FUNCTION,
    S_VARIABLE_DECL,
    S_EXPR,
    S_BLOCK,
    S_NONE,
} StmtType;

typedef struct Stmt Stmt;

typedef struct {
    Token* identifier;
    Stmt** fields;
} Struct;

typedef struct {
    Token* identifier;
    Stmt** params;
    DataType* return_type;
    Stmt* body;
    bool decl;
} Function;

typedef struct {
    Token* identifier;
    DataType* data_type;
    Expr* initializer;
    bool global;
    bool external;
} VariableDecl;

struct Stmt {
    StmtType type;
    union s {
        Struct struct_decl;
        Function function;
        VariableDecl variable_decl;
        Expr* expr;
        Stmt** block;
    } s;
};

Stmt stmt_new(void);
Stmt* stmt_new_alloc(void);

#endif /* _STMT_H */
