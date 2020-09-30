#include "token.h"
#include "expr.h"
#include <arpch.h>

Expr expr_new(void) {
    Expr expr;
    expr.type = E_NONE;
    return expr;
}

Expr* expr_new_alloc(void) {
    Expr* expr = malloc(sizeof(Expr));
    *expr = expr_new();
    return expr;
}

Expr* expr_variable_ref_from_token(Token* identifier) {
    Expr* expr = expr_new_alloc();
    expr->type = E_VARIABLE_REF;
    expr->variable_ref.identifier = identifier;
    expr->variable_ref.declaration = null;
    return expr;
}

Expr* expr_variable_ref_from_string(const char* identifier) {
    return expr_variable_ref_from_token(
            token_from_string_type(identifier, T_IDENTIFIER)
    );
}

