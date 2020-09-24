#ifndef _SCOPE_H
#define _SCOPE_H

struct Stmt;

typedef struct Scope {
	struct Scope* parent_scope;
	struct Stmt** variables;
} Scope;

typedef enum {
	VS_CURRENT_SCOPE,
	VS_OUTER_SCOPE,
	VS_NO_SCOPE,
} VariableScope;

Scope* scope_new(Scope* parent_scope);

#endif /* _SCOPE_H */

