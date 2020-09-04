#ifndef _STMT_H
#define _STMT_H

typedef enum {
    S_EXPR,
} StmtType;

typedef struct {
    StmtType type;
} Stmt;

#endif
