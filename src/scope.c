#include <scope.h>
#include <arpch.h>

Scope* scope_new(Scope* parent_scope) {
    Scope* scope = malloc(sizeof(Scope));
    scope->parent_scope = parent_scope;
    scope->variables = null;
    return scope;
}

