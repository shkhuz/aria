#ifndef _AST_H
#define _AST_H

#include "stmt.h"
#include "token.h"
#include <util/util.h>

typedef struct {
    File* srcfile;
    Token** tokens;
    Stmt** stmts;
    Stmt** decls;
    const char** imports;
} Ast;

#endif /* _AST_H */

