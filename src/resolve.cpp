struct Scope {
    Scope* parent;
    std::vector<Stmt*> stmts;
};

struct ResolveContext {
    Srcfile* srcfile;
    Scope* global_scope;
    Scope* current_scope;
    Stmt* current_func;
    std::unordered_map<Type*, Stmt*> slice_structs;
    bool error;
};

enum ScopeSearchStatus {
    SCOPE_LOCAL,
    SCOPE_PARENT,
    SCOPE_UNRESOLVED,
};

struct ScopeSearchResult {
    ScopeSearchStatus status;
    Stmt* stmt;
};

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

Scope* scope_new(Scope* parent) {
    ALLOC_WITH_TYPE(scope, Scope);
    scope->parent = parent;
    return scope;
}

ScopeSearchResult resolve_search_in_current_scope_rec(
        ResolveContext* r, 
        Token* identifier) {
    ScopeSearchResult result;
    result.status = SCOPE_UNRESOLVED;
    result.stmt = null;
    Scope* scope = r->current_scope;
    while (scope != null) {
        for (Stmt* stmt: scope->stmts) {
            if (is_token_lexeme_eq(identifier, stmt->main_token)) {
                if (scope == r->current_scope) 
                    result.status = SCOPE_LOCAL;
                else
                    result.status = SCOPE_PARENT;
                result.stmt = stmt;
                return result;
            }
        }
        scope = scope->parent;
    }
    return result;
}

Stmt* resolve_assert_symbol_is_in_current_scope_rec(
        ResolveContext* r, 
        Token* token) {
    BuiltinTypeKind kind = builtin_type_str_to_kind(token->lexeme);
    if (kind != BUILTIN_TYPE_KIND_NONE) {
        resolve_error(
                token,
                "cannot use type as a symbol");
        return null;
    }

    ScopeSearchResult search_result = 
        resolve_search_in_current_scope_rec(r, token);
    if (search_result.status == SCOPE_UNRESOLVED) {
        resolve_error(
                token,
                "unresolved symbol `{}`",
                token->lexeme);
        return null;
    }
    return search_result.stmt;
}

void resolve_cpush_in_scope(ResolveContext* r, Token* identifier, Stmt* stmt) {
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
                "redeclaration of symbol `{}`",
                identifier->lexeme);
        search_error = true;
    }
    else if (search_result.status == SCOPE_PARENT) {
        warning(
                identifier,
                "`{}` shadows symbol",
                identifier->lexeme);
    }

    if (search_result.status == SCOPE_LOCAL || 
        search_result.status == SCOPE_PARENT) {
        note(
                search_result.stmt->main_token,
                "previously declared here");
    }

    if (search_error) return;
    r->current_scope->stmts.push_back(stmt);
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
        case STMT_KIND_STRUCT: {
            resolve_cpush_in_scope(r, stmt->structure.identifier, stmt);
        } break;

        case STMT_KIND_FUNCTION: {
            resolve_cpush_in_scope(r, stmt->function.header->identifier, stmt); 
        } break;

        case STMT_KIND_VARIABLE: {
            if (!ignore_function_level_stmt) {
                resolve_cpush_in_scope(r, stmt->variable.identifier, stmt);
            }
        } break;
    }
}

void resolve_type(ResolveContext* r, Type* type);
void resolve_expr(ResolveContext* r, Expr* expr);
void resolve_stmt(
        ResolveContext* r, 
        Stmt* stmt, 
        bool ignore_function_level_stmt);

void resolve_type(ResolveContext* r, Type* type) {
    switch (type->kind) {
        case TYPE_KIND_BUILTIN: {
        } break;

        case TYPE_KIND_PTR: {
            resolve_type(r, type->ptr.child);
        } break;

        case TYPE_KIND_ARRAY: {
            resolve_type(r, type->array.elem_type);
        } break;

        case TYPE_KIND_CUSTOM: {
            switch (type->custom.kind) {
                case CUSTOM_TYPE_KIND_STRUCT: {
                    type->custom.ref = 
                        resolve_assert_symbol_is_in_current_scope_rec(r, type->custom.identifier);
                    if (type->custom.ref) {
                        if (type->custom.ref->kind != STMT_KIND_STRUCT) {
                            resolve_error(
                                    type->custom.identifier,
                                    "expected a type, but found `{}`",
                                    *type->custom.identifier);
                            note(
                                    type->custom.ref->main_token,
                                    "`{}` is defined here",
                                    *type->custom.identifier);
                            addinfo(
                                    "`{}` is not a type definition, so it cannot be used as a type",
                                    *type->custom.ref->main_token);
                        }
                    }
                } break;

                case CUSTOM_TYPE_KIND_SLICE: {
                    resolve_type(r, type->custom.slice.child);
                    Stmt* ref = null;
                    for (auto& it: r->slice_structs) {
                        if (type_is_equal(type, it.first, false)) {
                            ref = it.second;
                        }
                    }
                    if (!ref) {
                        std::vector<Stmt*> fields;
                        fields.push_back(field_stmt_new(
                                token_identifier_placeholder_new("ptr"),
                                ptr_type_placeholder_new(
                                    type->custom.slice.constant,
                                    type->custom.slice.child),
                                0));
                        fields.push_back(field_stmt_new(
                                token_identifier_placeholder_new("len"),
                                builtin_type_placeholders.uint64,
                                1));
                        ref = struct_stmt_new(
                                token_identifier_placeholder_new(fmt::format("{:n}", *type)),
                                fields,
                                true);
                        r->slice_structs[type] = ref;
                    }
                    type->custom.ref = ref;
                } break;
            }
        } break;
    }
}

void resolve_symbol_expr(ResolveContext* r, Expr* expr) {
    expr->symbol.ref = 
        resolve_assert_symbol_is_in_current_scope_rec(r, expr->main_token);
    if (expr->symbol.ref && expr->symbol.ref->kind == STMT_KIND_VARIABLE) {
        expr->constant = expr->symbol.ref->variable.constant;
    }
}

void resolve_function_call_expr(ResolveContext* r, Expr* expr) { 
    if (expr->function_call.callee->kind != EXPR_KIND_SYMBOL) {
        resolve_error(
                expr->main_token,
                "[internal] function ptr/methods calls not implemented yet");
        return;
    }

    resolve_symbol_expr(r, expr->function_call.callee);
    if (expr->function_call.callee->symbol.ref &&
        expr->function_call.callee->symbol.ref->kind !=
            STMT_KIND_FUNCTION) {
        resolve_error(
                expr->main_token,
                "callee is not a function");
    }

    for (Expr* arg: expr->function_call.args) {
        resolve_expr(r, arg);
    }
}

void resolve_binop_expr(ResolveContext* r, Expr* expr) {
    resolve_expr(r, expr->binop.left);
    resolve_expr(r, expr->binop.right);
}

void resolve_unop_expr(ResolveContext* r, Expr* expr) {
    resolve_expr(r, expr->unop.child);
    if (expr->unop.op->kind == TOKEN_KIND_AMP) {
        if (expr->unop.child->kind != EXPR_KIND_SYMBOL) {
            resolve_error(
                    expr->main_token,
                    "`&` only accepts l-values as operands");
        }
    }
}

void resolve_cast_expr(ResolveContext* r, Expr* expr) {
    resolve_expr(r, expr->cast.left);
    resolve_type(r, expr->cast.to);
}

void resolve_block_expr(
        ResolveContext* r, 
        Expr* expr, 
        bool create_new_scope) {

    Scope* scope = null;
    if (create_new_scope) {
        scope_push_existing(scope);
    }

    for (Stmt* stmt: expr->block.stmts) {
        resolve_pre_decl_stmt(r, stmt, true);
    }
    for (Stmt* stmt: expr->block.stmts) {
        resolve_stmt(r, stmt, false);
    }
    if (expr->block.value) {
        resolve_expr(r, expr->block.value);
    }

    if (create_new_scope) {
        scope_pop(scope);
    }
}

void resolve_if_branch(ResolveContext* r, IfBranch* br) {
    if (br->cond) {
        resolve_expr(r, br->cond);
    }
    resolve_expr(r, br->body);
}

void resolve_if_expr(ResolveContext* r, Expr* expr) {
    resolve_if_branch(r, expr->iff.ifbr);
    for (IfBranch* br: expr->iff.elseifbr) {
        resolve_if_branch(r, br);
    }
    if (expr->iff.elsebr) {
        resolve_if_branch(r, expr->iff.elsebr);
    }
}

void resolve_while_expr(ResolveContext* r, Expr* expr) {
    resolve_expr(r, expr->whilelp.cond);
    resolve_expr(r, expr->whilelp.body);
}

void resolve_expr(ResolveContext* r, Expr* expr) {
    expr->parent_func = r->current_func;
    if ((expr->kind == EXPR_KIND_SYMBOL || 
         expr->kind == EXPR_KIND_FIELD_ACCESS || 
         expr->kind == EXPR_KIND_BLOCK || 
         expr->kind == EXPR_KIND_IF || 
         (expr->kind == EXPR_KIND_UNOP && expr->unop.op->kind == TOKEN_KIND_STAR) || 
         (expr->kind == EXPR_KIND_UNOP && expr->unop.op->kind == TOKEN_KIND_AMP) || 
         expr->kind == EXPR_KIND_FUNCTION_CALL) &&
        r->current_func == null) {
        resolve_error(
                expr->main_token,
                "only constant expressions are allowed for globals");
        return;
    }

    switch (expr->kind) {
        case EXPR_KIND_ARRAY_LITERAL: {
            for (Expr* elem: expr->arraylit.elems) {
                resolve_expr(r, elem);
            }
        } break;

        case EXPR_KIND_SYMBOL: {
            resolve_symbol_expr(r, expr);
        } break;

        case EXPR_KIND_FUNCTION_CALL: {
            resolve_function_call_expr(r, expr);
        } break;

        case EXPR_KIND_FIELD_ACCESS: {
            resolve_expr(r, expr->fieldacc.left);
        } break;

        case EXPR_KIND_INDEX: {
            resolve_expr(r, expr->index.left);
            resolve_expr(r, expr->index.idx);
        } break;

        case EXPR_KIND_BINOP: {
            resolve_binop_expr(r, expr);
        } break;
        
        case EXPR_KIND_UNOP: {
            resolve_unop_expr(r, expr);
        } break;
        
        case EXPR_KIND_CAST: {
            resolve_cast_expr(r, expr);
        } break;
        
        case EXPR_KIND_BLOCK: {
            resolve_block_expr(r, expr, true);
        } break;

        case EXPR_KIND_IF: {
            resolve_if_expr(r, expr);
        } break;
        
        case EXPR_KIND_WHILE: {
            resolve_while_expr(r, expr);
        } break;
    }
}

void resolve_function_header(ResolveContext* r, FunctionHeader* header) {
    for (Stmt* param: header->params) {
        resolve_cpush_in_scope(r, param->param.identifier, param);
        resolve_type(r, param->param.type);
    }
    resolve_type(r, header->return_type);
}

void resolve_function_stmt(ResolveContext* r, Stmt* stmt) {
    if (!stmt->function.header->is_extern) r->current_func = stmt;
    scope_push(scope);
    resolve_function_header(r, stmt->function.header);
    if (!stmt->function.header->is_extern) {
        resolve_block_expr(r, stmt->function.body, false);
    }
    scope_pop(scope);
    if (!stmt->function.header->is_extern) r->current_func = null;
}

void resolve_variable_stmt(
        ResolveContext* r,
        Stmt* stmt,
        bool ignore_function_level_stmt) {

    if (!ignore_function_level_stmt) {
        resolve_cpush_in_scope(r, stmt->variable.identifier, stmt);
    }

    if (!stmt->variable.type && !stmt->variable.initializer) {
        resolve_error(
                stmt->main_token,
                "no type annotations provided");
        addinfo("consider adding a type: `let {}: i32`",
                *stmt->variable.identifier);
        addinfo("or assign a default value: `let {} = 0`",
                *stmt->variable.identifier);
        return;
    }

    if (stmt->variable.type) {
        resolve_type(r, stmt->variable.type);
    }

    if (stmt->variable.initializer) {
        resolve_expr(r, stmt->variable.initializer);
    }
    else if (ignore_function_level_stmt && !stmt->variable.is_extern) {
        resolve_error(
                stmt->main_token,
                "global variable initialization is mandatory");
    }

    if (!ignore_function_level_stmt) {
        r->current_func->function.locals.push_back(stmt);
    }
    else {
        stmt->variable.global = true;
    }
}

void resolve_assign_stmt(ResolveContext* r, Stmt* stmt) {
    resolve_expr(r, stmt->assign.left);
    resolve_expr(r, stmt->assign.right);

    if ((stmt->assign.left->kind == EXPR_KIND_SYMBOL && 
         stmt->assign.left->symbol.ref &&
         stmt->assign.left->symbol.ref->kind == STMT_KIND_VARIABLE) ||
        (stmt->assign.left->kind == EXPR_KIND_UNOP &&
         stmt->assign.left->unop.op->kind == TOKEN_KIND_STAR)) {
        if (stmt->assign.left->constant) {
            resolve_error(
                    stmt->main_token,
                    "cannot modify immutable constant");
            if (stmt->assign.left->kind == EXPR_KIND_SYMBOL && 
                stmt->assign.left->symbol.ref &&
                stmt->assign.left->symbol.ref->kind == STMT_KIND_VARIABLE) {
                note(
                        stmt->assign.left->symbol.ref->main_token,
                        "variable defined here");
                addinfo("to define a mutable variable, use `var`");
            }
        }
    }
    else if (stmt->assign.left->kind == EXPR_KIND_FIELD_ACCESS) {
        // This is left empty because we check a field access in the 
        // type_chk phase.
    }
    else if (stmt->assign.left->kind == EXPR_KIND_INDEX) {
        // This is left empty because we need LHS type info to determine 
        // the `constant` field of index expr.
    }
    else {
        resolve_error(
                stmt->main_token,
                "invalid l-value");
    }
}

void resolve_expr_stmt(ResolveContext* r, Stmt* stmt) {
    resolve_expr(r, stmt->expr.child);
}

void resolve_stmt(
        ResolveContext* r, 
        Stmt* stmt, 
        bool ignore_function_level_stmt) {
    stmt->parent_func = r->current_func;
    switch (stmt->kind) {
        case STMT_KIND_STRUCT: {
            scope_push(scope);
            for (Stmt* field: stmt->structure.fields) {
                resolve_cpush_in_scope(r, field->field.identifier, field);
                resolve_type(r, field->field.type);
            }
            scope_pop(scope);
        } break;

        case STMT_KIND_FUNCTION: {
            resolve_function_stmt(r, stmt);
        } break;

        case STMT_KIND_VARIABLE: {
            resolve_variable_stmt(r, stmt, ignore_function_level_stmt);
        } break;

        case STMT_KIND_ASSIGN: {
            resolve_assign_stmt(r, stmt);
        } break;

        case STMT_KIND_EXPR: {
            resolve_expr_stmt(r, stmt);
        } break;
    }
}

void resolve(ResolveContext* r) {
    r->global_scope = scope_new(null);
    r->current_scope = r->global_scope;
    r->current_func = null;
    r->error = false;

    for (size_t i = 0; i < r->srcfile->stmts.size(); i++) {
        resolve_pre_decl_stmt(r, r->srcfile->stmts[i], false);
    }
    for (size_t i = 0; i < r->srcfile->stmts.size(); i++) {
        resolve_stmt(r, r->srcfile->stmts[i], true);
    }

    r->srcfile->stmts.reserve(r->srcfile->stmts.size() + r->slice_structs.size());
    for (auto& it: r->slice_structs) {
        r->srcfile->stmts.push_back(it.second);
    }
}

