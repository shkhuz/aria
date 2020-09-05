#include <expr.h>
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
