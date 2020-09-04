#ifndef _STMT_H
#define _STMT_H

typedef enum {
    S_BINARY,
    S_UNARY,
} StmtType;

typedef struct {
    StmtType type;
} Stmt;

#endif
