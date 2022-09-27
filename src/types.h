#ifndef TYPES_H
#define TYPES_H

typedef struct {
    File handle;
    Token** tokens;
    Stmt** stmts;
} Srcfile;

#endif
