#include <stmt.h>
#include <arpch.h>

Param param_new(Token* identifier, DataType* data_type) {
    Param param;
    param.identifier = identifier;
    param.data_type = data_type;
    return param;
}

Param* param_new_alloc(Token* identifier, DataType* data_type) {
    Param* param = malloc(sizeof(Param));
    *param = param_new(identifier, data_type);
    return param;
}

Stmt stmt_new(void) {
    Stmt stmt;
    stmt.type = S_NONE;
    return stmt;
}

Stmt* stmt_new_alloc(void) {
    Stmt* stmt = malloc(sizeof(Stmt));
    *stmt = stmt_new();
    return stmt;
}
