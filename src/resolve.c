typedef struct Scope {
    struct Scope* parent;
    Node** nodes;
} Scope;

Scope* scope_from_scope(Scope* parent) {
    alloc_with_type(scope, Scope);
    scope->parent = parent;
    scope->nodes = null;
    return scope;
}

#define resolver_create_scope(self, name) \
    Scope* name = scope_from_scope(self->current_scope); \
    self->current_scope = name;

#define resolver_init_scope(self, name) \
    name = scope_from_scope(self->current_scope); \
    self->current_scope = name;

#define resolver_revert_scope(self, name) \
    self->current_scope = name->parent;

typedef struct {
    SrcFile* srcfile;
    Scope* global_scope;
    Scope* current_scope;
    bool error;
} Resolver;

void resolver_pre_decl_node(
        Resolver* self,
        Node* node,
        bool ignore_procedure_level_decl_node);

void resolver_node(
        Resolver* self,
        Node* node,
        bool ignore_procedure_level_decl_node_pre_check);

void resolver_type(Resolver* self, Node* node);

typedef enum {
    SCOPE_UNRESOLVED,
    SCOPE_LOCAL,
    SCOPE_PARENT,
} ScopeStatusKind;

typedef struct {
    ScopeStatusKind kind;

    // This is populated only if the `kind` is 
    // SCOPE_LOCAL or SCOPE_PARENT. For SCOPE_UNRESOLVED,
    // the value is `null`. It doesn't make 
    // sense to have a nonexistent node!
    Node* found_node;
} ScopeStatus;

ScopeStatus resolver_search_in_all_scope(Resolver* self, Token* identifier) {
    Scope* scope = self->current_scope;
    while (scope != null) {
        buf_loop(scope->nodes, i) {
            Token* scope_identifier = node_get_identifier(
                    scope->nodes[i],
                    true);
            assert(scope_identifier);

            if (token_lexeme_eq(identifier, scope_identifier)) {
                return (ScopeStatus) {
                    (scope == self->current_scope ? SCOPE_LOCAL : SCOPE_PARENT),
                    scope->nodes[i],
                };
            }
        }
        scope = scope->parent;
    }
    return (ScopeStatus){ SCOPE_UNRESOLVED, null };
}

// Pushes a node into the scope symbol table 
// if not already pushed. If redeclaration occurs 
// in the local scope, it raises an error and 
// returns.
void resolver_cpush_in_scope(Resolver* self, Node* node) {
    Token* identifier = node_get_identifier(node, true);
    assert(identifier);

    ScopeStatus status = resolver_search_in_all_scope(self, identifier);
    if (status.kind == SCOPE_LOCAL) {
        error_node(
                node,
                "redeclaration of symbol `%s`",
                identifier->lexeme);
        note_node(
                status.found_node,
                "...previously declared here");
        return;
    }

    buf_push(self->current_scope->nodes, node);
}

// TODO: should this accept a `Node` or a `Token`?
Node* resolver_assert_in_scope(Resolver* self, Node* node) {
    assert(node->kind == NODE_KIND_SYMBOL);

    Token* identifier = node->symbol.identifier;
    ScopeStatus status = resolver_search_in_all_scope(self, identifier);
    if (status.kind == SCOPE_UNRESOLVED) {
        error_node(
                node,
                "unresolved symbol `%s`",
                identifier->lexeme);
        return null;
    }
    return status.found_node;
}

void resolver_procedure_call(Resolver* self, Node* node) {
    if (node->procedure_call.callee->kind != NODE_KIND_SYMBOL) {
        error_node(
                node->procedure_call.callee,
                "callee must be an identifier");
    } else {
        resolver_node(self, node->procedure_call.callee, true);
    }

    buf_loop(node->procedure_call.args, i) {
        resolver_node(self, node->procedure_call.args[i], true);
    }
}

void resolver_symbol(Resolver* self, Node* node) {
    node->symbol.ref = resolver_assert_in_scope(self, node);
}

void resolver_block(Resolver* self, Node* node, bool create_new_scope) {
    Scope* scope = null;
    if (create_new_scope) {
        resolver_init_scope(self, scope);
    }

    buf_loop(node->block.nodes, i) {
        resolver_pre_decl_node(self, node->block.nodes[i], true);
    }

    buf_loop(node->block.nodes, i) {
        resolver_node(self, node->block.nodes[i], false);
    }

    if (create_new_scope) {
        resolver_revert_scope(self, scope);
    }
}

void resolver_param(
        Resolver* self, 
        Node* node,
        bool ignore_procedure_level_decl_node_pre_check) {
    if (!ignore_procedure_level_decl_node_pre_check) {
        resolver_cpush_in_scope(self, node);
    }

    resolver_type(self, node->param.type);
}

void resolver_variable_decl(
        Resolver* self, 
        Node* node,
        bool ignore_procedure_level_decl_node_pre_check) {
    if (!ignore_procedure_level_decl_node_pre_check) {
        resolver_cpush_in_scope(self, node);
    }

    if (!node->variable_decl.type && !node->variable_decl.initializer) {
        error_node(
                node,
                "type must be annotated or an initializer must be provided");
        return;
    }

    if (node->variable_decl.type) {
        resolver_type(self, node->variable_decl.type);
    }

    if (node->variable_decl.initializer) {
        resolver_node(self, node->variable_decl.initializer, true);
    }
}

void resolver_procedure_decl(Resolver* self, Node* node) {
    resolver_create_scope(self, scope);
    buf_loop(node->procedure_decl.params, i) {
        resolver_node(self, node->procedure_decl.params[i], false);
    }

    resolver_type(self, node->procedure_decl.type);
    resolver_block(self, node->procedure_decl.body, false);

    resolver_revert_scope(self, scope);
}

// This function checks if the node 
// isn't already declared, rather than 
// checking the node itself.
// The `ignore_procedure_level_decl_node_pre_check`
// parameter tells the function whether to check
// variables are already defined or not. 
// If the value is `true`, the check is ignored.
// This parameter is useful for when checking top-
// level declarations, where we want to check for 
// variable redeclarations in one-pass, to have 
// declaration-independancy.
// The `false` value is useful for when we are 
// checking variables inside a procedure, where 
// variables are defined top-to-botton and not 
// position-independantly.
void resolver_pre_decl_node(
        Resolver* self,
        Node* node,
        bool ignore_procedure_level_decl_node) {
    switch (node->kind) {
        case NODE_KIND_PROCEDURE_DECL:
        {
            resolver_cpush_in_scope(self, node);
        } break;

        case NODE_KIND_VARIABLE_DECL:
        case NODE_KIND_PARAM:
        {
            if (!ignore_procedure_level_decl_node) {
                resolver_cpush_in_scope(self, node);
            }
        } break;

        case NODE_KIND_EXPR_STMT:
        case NODE_KIND_BLOCK:
        {
            // DO NOTHING
        } break;

        case NODE_KIND_NUMBER:
        case NODE_KIND_TYPE:
        case NODE_KIND_SYMBOL:
        case NODE_KIND_PROCEDURE_CALL:
        {
            assert(0);
        } break;
    }
}

void resolver_type_base(Resolver* self, Node* node) {
    Token* identifier = node->type.base.symbol->symbol.identifier;

    buf_loop(builtin_types, i) {
        if (builtin_types[i]->type.kind == TYPE_KIND_BASE) {
            if (token_lexeme_eq(
                        builtin_types[i]->type.base.symbol->symbol.identifier,
                        identifier)) {
                return;
            }
        }
    }

    error_node(
            node,
            "unresolved type `%s`",
            identifier->lexeme);
}

void resolver_type_ptr(Resolver* self, Node* node) {
    resolver_type(self, node->type.ptr.right);
}

void resolver_type(Resolver* self, Node* node) {
    switch (node->type.kind) {
        case TYPE_KIND_BASE:
        {
            resolver_type_base(self, node);
        } break;

        case TYPE_KIND_PTR:
        {
            resolver_type_ptr(self, node);
        } break;
    }
}

// This function actually resolves the nodes, 
// instead of checking for redeclaration. 
// Refer to `resolver_pre_decl_node`'s doc comment.
void resolver_node(
        Resolver* self,
        Node* node,
        bool ignore_procedure_level_decl_node_pre_check) {
    switch (node->kind) {
        case NODE_KIND_PROCEDURE_DECL:
        {
            resolver_procedure_decl(self, node);
        } break;

        case NODE_KIND_VARIABLE_DECL:
        {
            resolver_variable_decl(
                    self, 
                    node, 
                    ignore_procedure_level_decl_node_pre_check);
        } break;

        case NODE_KIND_PARAM:
        {
            resolver_param(
                    self, 
                    node, 
                    ignore_procedure_level_decl_node_pre_check);
        } break;

        case NODE_KIND_EXPR_STMT:
        {
            resolver_node(self, node->expr_stmt.expr, true);
        } break;

        case NODE_KIND_PROCEDURE_CALL:
        {
            resolver_procedure_call(self, node);
        } break;
        
        case NODE_KIND_SYMBOL:
        {
            resolver_symbol(self, node);
        } break;

        case NODE_KIND_BLOCK:
        {
            resolver_block(self, node, true);
        } break;

        case NODE_KIND_TYPE:
        {
            resolver_type(self, node);
        } break;

        case NODE_KIND_NUMBER:
        {
        } break;
    }
}

void resolver_init(Resolver* self, SrcFile* srcfile) {
    self->srcfile = srcfile;
    self->global_scope = scope_from_scope(null);
    self->current_scope = self->global_scope;
    self->error = false;
}

void resolver_resolve(Resolver* self) {
    buf_loop(self->srcfile->nodes, i) {
        resolver_pre_decl_node(self, self->srcfile->nodes[i], false);
    }

    buf_loop(self->srcfile->nodes, i) {
        resolver_node(self, self->srcfile->nodes[i], true);
    }
}

