#include "resolve.h"
#include "buf.h"
#include "stri.h"
#include "msg.h"

// TODO: scope memory management
#define scope_push(name) \
    Scope* name = scope_new(r->current_scope); \
    r->current_scope = name;

#define scope_push_existing(name) \
    name = scope_new(r->current_scope); \
    r->current_scope = name;

#define scope_pop(name) \
    r->current_scope = name->parent;

#define resolve_error(token, fmt, ...) \
    error(token, fmt, ##__VA_ARGS__); \
    r->error = true

typedef enum {
    SCOPE_LOCAL,
    SCOPE_PARENT,
    SCOPE_UNRESOLVED,
} ScopeSearchStatus;

typedef struct {
    ScopeSearchStatus status;
    Stmt* stmt;
} ScopeSearchResult;

static void resolve_pre_decl_stmt(
        ResolveContext* r, 
        Stmt* stmt, 
        bool ignore_function_level_stmt);
static void resolve_cpush_in_scope(ResolveContext* r, Stmt* stmt);
static ScopeSearchResult resolve_search_in_current_scope_rec(
        ResolveContext* r, 
        Token* identifier);
static void resolve_stmt(
        ResolveContext* r, 
        Stmt* stmt, 
        bool ignore_function_level_stmt);
static void resolve_function_stmt(ResolveContext* r, Stmt* stmt);
static void resolve_function_header(ResolveContext* r, FunctionHeader* header);
static void resolve_param_stmt(ResolveContext* r, Stmt* stmt);
static void resolve_variable_stmt(
        ResolveContext* r,
        Stmt* stmt,
        bool ignore_function_level_stmt);
static void resolve_expr_stmt(ResolveContext* r, Stmt* stmt);
static void resolve_expr(ResolveContext* r, Expr* expr);
static void resolve_block_expr(
        ResolveContext* r, 
        Expr* expr, 
        bool create_new_scope);
static void resolve_type(ResolveContext* r, Type* type);
static void resolve_ptr_type(ResolveContext* r, Type* type);
static Scope* scope_new(Scope* parent);

void resolve(ResolveContext* r) {
    r->global_scope = scope_new(null);
    r->current_scope = r->global_scope;
    r->current_function = null;
    r->error = false;

    buf_loop(r->srcfile->stmts, i) {
        resolve_pre_decl_stmt(r, r->srcfile->stmts[i], false);
    }
    buf_loop(r->srcfile->stmts, i) {
        resolve_stmt(r, r->srcfile->stmts[i], true);
    }
}

// This function checks if the stmt isn't already declared, rather than 
// checking the stmt itself.  The `ignore_function_level_stmt` parameter 
// tells the function whether to check variables are already defined or not. 
// If the value is `true`, the check is ignored.  This parameter is useful 
// for when checking top- level declarations, where we want to check for 
// variable redeclarations in one-pass, to have order-independant declarations.
// The `false` value is useful for when we are checking variables inside a 
// function, where variables are defined top-to-botton and not 
// order-independantly.
void resolve_pre_decl_stmt(
        ResolveContext* r, 
        Stmt* stmt, 
        bool ignore_function_level_stmt) {

    switch (stmt->kind) {
        case STMT_KIND_FUNCTION: {
            resolve_cpush_in_scope(r, stmt); 
        } break;

        case STMT_KIND_VARIABLE: {
            if (!ignore_function_level_stmt) {
                resolve_cpush_in_scope(r, stmt);
            }
        } break;
    }
}

void resolve_cpush_in_scope(ResolveContext* r, Stmt* stmt) {
    Token* identifier = stmt->main_token;
    BuiltinTypeKind kind = builtin_type_str_to_kind(identifier->lexeme);
    if (kind != BUILTIN_TYPE_KIND_NONE) {
        resolve_error(
                identifier,
                "cannot use type as a symbol");
        return;
    }

    bool search_error = false;
    ScopeSearchResult search_result = 
        resolve_search_in_current_scope_rec(r, identifier);
    if (search_result.status == SCOPE_LOCAL) {
        resolve_error(
                identifier,
                "redeclaration of symbol `%s`",
                identifier->lexeme);
        search_error = true;
    }
    else if (search_result.status == SCOPE_PARENT) {
        warning(
                identifier,
                "`%s` shadows symbol",
                identifier->lexeme);
    }

    if (search_result.status == SCOPE_LOCAL || 
        search_result.status == SCOPE_PARENT) {
        note(
                search_result.stmt->main_token,
                "...previously declared here");
    }

    if (search_error) return;
    buf_push(r->current_scope->stmts, stmt);
}

ScopeSearchResult resolve_search_in_current_scope_rec(
        ResolveContext* r, 
        Token* identifier) {

    ScopeSearchResult result;
    result.status = SCOPE_UNRESOLVED;
    result.stmt = null;
    Scope* scope = r->current_scope;
    while (scope != null) {
        buf_loop(scope->stmts, i) {
            if (is_token_lexeme_eq(identifier, scope->stmts[i]->main_token)) {
                if (scope == r->current_scope) 
                    result.status = SCOPE_LOCAL;
                else
                    result.status = SCOPE_PARENT;
                result.stmt = scope->stmts[i];
                return result;
            }
        }
        scope = scope->parent;
    }
    return result;
}

void resolve_stmt(
        ResolveContext* r, 
        Stmt* stmt, 
        bool ignore_function_level_stmt) {

    switch (stmt->kind) {
        case STMT_KIND_FUNCTION: {
            resolve_function_stmt(r, stmt);
        } break;

        case STMT_KIND_VARIABLE: {
            resolve_variable_stmt(r, stmt, ignore_function_level_stmt);
        } break;

        case STMT_KIND_EXPR: {
            resolve_expr_stmt(r, stmt);
        } break;
    }
}

void resolve_function_stmt(ResolveContext* r, Stmt* stmt) {
    r->current_function = stmt;
    scope_push(scope);
    resolve_function_header(r, stmt->function.header);
    resolve_block_expr(r, stmt->function.body, false);
    scope_pop(scope);
    r->current_function = null;
}

void resolve_function_header(ResolveContext* r, FunctionHeader* header) {
    buf_loop(header->params, i) {
        resolve_param_stmt(r, header->params[i]);
    }
    resolve_type(r, header->return_type);
}

void resolve_param_stmt(ResolveContext* r, Stmt* stmt) {
    resolve_cpush_in_scope(r, stmt);
    resolve_type(r, stmt->param.type);
}

void resolve_variable_stmt(
        ResolveContext* r,
        Stmt* stmt,
        bool ignore_function_level_stmt) {

    if (!ignore_function_level_stmt) {
        resolve_cpush_in_scope(r, stmt);
    }

    if (!stmt->variable.type && !stmt->variable.initializer) {
        resolve_error(
                stmt->main_token,
                "no type annotations provided");
        return;
    }

    if (stmt->variable.type) {
        resolve_type(r, stmt->variable.type);
    }

    if (stmt->variable.initializer) {
        resolve_expr(r, stmt->variable.initializer);
    }
    stmt->variable.parent_func = r->current_function;
}

void resolve_expr_stmt(ResolveContext* r, Stmt* stmt) {
    resolve_expr(r, stmt->expr.child);
}

void resolve_expr(ResolveContext* r, Expr* expr) {
    switch (expr->kind) {
        case EXPR_KIND_BLOCK: {
            resolve_block_expr(r, expr, true);
        } break;
    }
}

void resolve_block_expr(
        ResolveContext* r, 
        Expr* expr, 
        bool create_new_scope) {

    Scope* scope = null;
    if (create_new_scope) {
        scope_push_existing(scope);
    }

    buf_loop(expr->block.stmts, i) {
        resolve_pre_decl_stmt(r, expr->block.stmts[i], true);
    }
    buf_loop(expr->block.stmts, i) {
        resolve_stmt(r, expr->block.stmts[i], false);
    }
    if (expr->block.value) {
        resolve_expr(r, expr->block.value);
    }

    if (create_new_scope) {
        scope_pop(scope);
    }
}

void resolve_type(ResolveContext* r, Type* type) {
    switch (type->kind) {
        case TYPE_KIND_BUILTIN: {
        } break;

        case TYPE_KIND_PTR: {
            resolve_ptr_type(r, type);
        } break;
    }
}

void resolve_ptr_type(ResolveContext* r, Type* type) {
    resolve_type(r, type->ptr.child);
}

Scope* scope_new(Scope* parent) {
    ALLOC_WITH_TYPE(scope, Scope);
    scope->parent = parent;
    scope->stmts = null;
    return scope;
}
