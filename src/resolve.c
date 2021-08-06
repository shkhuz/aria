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
    Node* current_procedure;
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

Node* resolver_search_in_specific_scope(
        Resolver* self, 
        Token* identifier, 
        Node** scope) {
    buf_loop(scope, i) {
        Token* scope_identifier = node_get_main_token(
                scope[i],
                true);
        assert(scope_identifier);

        if (token_lexeme_eq(identifier, scope_identifier)) {
            return scope[i];
        }
    }
    return null;
}

ScopeStatus resolver_search_in_all_scope(Resolver* self, Token* identifier) {
    Scope* scope = self->current_scope;
    while (scope != null) {
        buf_loop(scope->nodes, i) {
            Token* scope_identifier = node_get_main_token(
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
    Token* identifier = node_get_main_token(node, true);
    assert(identifier);

    bool error = false;
    ScopeStatus status = resolver_search_in_all_scope(self, identifier);
    if (status.kind == SCOPE_LOCAL) {
        error_node(
                node,
                "redeclaration of symbol `%s`",
                identifier->lexeme);
        error = true;
    } else if (status.kind == SCOPE_PARENT) {
        warning_node(
                node,
                "`%s` shadows symbol",
                identifier->lexeme);
    }

    if (status.kind == SCOPE_LOCAL ||
        status.kind == SCOPE_PARENT) {
        note_node(
                status.found_node,
                "...previously declared here");
    }

    if (error) return;
    buf_push(self->current_scope->nodes, node);
}

#define ERROR_UNRESOLVED_SYMBOL "unresolved symbol"

Node* resolver_assert_in_specific_scope(
        Resolver* self,
        Token* identifier,
        Node** scope) {
    Node* node = resolver_search_in_specific_scope(
            self,
            identifier,
            scope);
    if (!node) {
        error_token(
                identifier,
                ERROR_UNRESOLVED_SYMBOL);
    }
    return node;
}

// TODO: should this accept a `Node` or a `Token`?
Node* resolver_assert_in_scope(Resolver* self, Node* node) {
    assert(node->kind == NODE_KIND_SYMBOL);

    Token* identifier = node->symbol.identifier;
    ScopeStatus status = resolver_search_in_all_scope(self, identifier);
    if (status.kind == SCOPE_UNRESOLVED) {
        error_node(
                node,
                ERROR_UNRESOLVED_SYMBOL);
        return null;
    }
    return status.found_node;
}

#define ERROR_MODULE_NOT_FOUND "module not found in scope"

Node** resolver_get_static_accessor_module(
        Resolver* self, 
        Node* static_accessor) {
    Token** accessors = static_accessor->static_accessor.accessors;
    Scope* scope = self->current_scope;
    printf("%lu\n", buf_len(scope->nodes));
    Node** module = null;

    if (static_accessor->static_accessor.from_global_scope) {
        module = self->srcfile->nodes;
    } else {
        bool first_accessor_found = false;
        while (scope != null) {
            buf_loop(scope->nodes, i) {
                if (scope->nodes[i]->kind == NODE_KIND_IMPLICIT_MODULE && 
                    stri(scope->nodes[i]->implicit_module.identifier->lexeme) ==
                    stri(accessors[0]->lexeme)) {
                    module = scope->nodes[i]->implicit_module.srcfile->nodes;
                    first_accessor_found = true;
                    break;
                }
            }
            if (first_accessor_found) break;
            scope = scope->parent;
        }

        if (module == null) {
            error_token(
                    accessors[0],
                    ERROR_MODULE_NOT_FOUND);
            return null;
        }
    }

    size_t start_idx = 
        (static_accessor->static_accessor.from_global_scope ? 0 : 1);
    for (size_t i = start_idx; i < buf_len(accessors); i++) {
        bool found = false;
        buf_loop(module, j) {
            if (module[j]->kind == NODE_KIND_IMPLICIT_MODULE &&
                stri(module[j]->implicit_module.identifier->lexeme) == 
                stri(accessors[i]->lexeme)) {
                found = true;
                module = module[j]->implicit_module.srcfile->nodes;
                break;
            }
        }

        if (!found) {
            error_token(
                    accessors[i],
                    ERROR_MODULE_NOT_FOUND);
            return null;
        }
    }
    return module;
}

Node* resolver_assert_static_accessor_ident_in_scope(
        Resolver* self, 
        Node* node) {
    assert(node->kind == NODE_KIND_SYMBOL);

    if (node->symbol.static_accessor->static_accessor.accessors) {
        Node** module = resolver_get_static_accessor_module(
                self, 
                node->symbol.static_accessor);
        if (module) {
            return resolver_assert_in_specific_scope(
                    self, 
                    node->symbol.identifier,
                    module);
        } else return null;
    }

    return resolver_assert_in_scope(self, node);
}

void resolver_procedure_call(Resolver* self, Node* node) {
    if (node->procedure_call.callee->kind != NODE_KIND_SYMBOL) {
        error_node(
                node->procedure_call.callee,
                "callee must be an identifier");
        return;
    }

    node->procedure_call.callee->symbol.ref = 
        resolver_assert_static_accessor_ident_in_scope(
            self,
            node->procedure_call.callee);
    if (node->procedure_call.callee->symbol.ref &&
        node->procedure_call.callee->symbol.ref->kind != 
            NODE_KIND_PROCEDURE_DECL) {
        error_node(
                node->procedure_call.callee,
                "expected procedure, got %s",
                node_get_name_in_word(node->procedure_call.callee->symbol.ref));
    }

    buf_loop(node->procedure_call.args, i) {
        resolver_node(self, node->procedure_call.args[i], true);
    }
}

void resolver_symbol(Resolver* self, Node* node) {
    node->symbol.ref = resolver_assert_static_accessor_ident_in_scope(
            self, 
            node);
    if (node->symbol.ref &&
        node->symbol.ref->kind != NODE_KIND_VARIABLE_DECL &&
        node->symbol.ref->kind != NODE_KIND_PARAM) {
        error_node(
                node,
                "%s is not expected here",
                node_get_name_in_word(node->symbol.ref));
    }
}

void resolver_expr_unary(Resolver* self, Node* node) {
    resolver_node(self, node->unary.right, false);
    if (node->unary.op->kind == TOKEN_KIND_AMPERSAND) {
        if (node->unary.right->kind == NODE_KIND_SYMBOL) {
            if (node->unary.right->symbol.ref &&
                (node->unary.right->symbol.ref->kind == 
                    NODE_KIND_VARIABLE_DECL ||
                node->unary.right->symbol.ref->kind == 
                    NODE_KIND_PARAM)) {
                // Do nothing 'cause it will be checked by
                // the analyzer symbol checker.
            }
        } else {
            error_node(
                    node->unary.right,
                    "invalid r-value");
        }
    }
}

// TODO: (NodeKind) check child expression kind or
// add to list if new expression kinds are added
void resolver_expr_deref(Resolver* self, Node* node) {
    resolver_node(self, node->deref.right, false);
    if (node->deref.right->kind == NODE_KIND_SYMBOL) {
        if (node->deref.right->symbol.ref &&
            (node->deref.right->symbol.ref->kind == 
                NODE_KIND_VARIABLE_DECL ||
            node->deref.right->symbol.ref->kind == 
                NODE_KIND_PARAM)) {
            // Do nothing 'cause it will be checked by
            // the analyzer symbol checker.
        }
    } else if (node->deref.right->kind == NODE_KIND_PROCEDURE_CALL) {
    } else if (node->deref.right->kind == NODE_KIND_BLOCK) {
    } else if (node->deref.right->kind == NODE_KIND_IF_EXPR) {
    } else if (node->deref.right->kind == NODE_KIND_UNARY &&
               node->deref.right->unary.op->kind == TOKEN_KIND_STAR) {
    } else if (node->deref.right->kind == NODE_KIND_DEREF) {
    } else if (node->deref.right->kind == NODE_KIND_CAST) {
    } else if (node->deref.right->kind == NODE_KIND_BINARY) {
    } else if (node->deref.right->kind == NODE_KIND_ASSIGN) {
    } else {
        error_node(
                node->deref.right,
                "invalid r-value");
    }
}

void resolver_expr_cast(Resolver* self, Node* node) {
    resolver_node(self, node->cast.left, false);
    resolver_type(self, node->cast.right);
}

void resolver_expr_binary(Resolver* self, Node* node) {
    resolver_node(self, node->binary.left, false);
    resolver_node(self, node->binary.right, false);
}

void resolver_expr_assign(Resolver* self, Node* node) {
    resolver_node(self, node->assign.left, false);
    resolver_node(self, node->assign.right, false);

    if (node->assign.left->kind == NODE_KIND_SYMBOL) {
        if (node->assign.left->symbol.ref &&
            node->assign.left->symbol.ref->kind == NODE_KIND_VARIABLE_DECL) {
            if (node->assign.left->symbol.ref->variable_decl.constant) {
                error_node(
                        node->assign.left,
                        "cannot modify immutable variable");
                note_node(
                    node->assign.left->symbol.ref,
                    "...variable declared here");
            }
        } else {
            /* error_node( */
            /*         node->assign.left, */
            /*         "l-value is a %s", */
            /*         node_get_name_in_word(node->assign.left->symbol.ref)); */
            // This else-branch is left blank because it will already
            // print the correct error message in resolver_node() call above.
        }
    } else if (node->assign.left->kind == NODE_KIND_DEREF) {
    } else {
        error_node(
                node->assign.left,
                "invalid l-value");
    }
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

    if (node->block.return_value) {
        resolver_node(self, node->block.return_value, false);
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

void resolver_return(Resolver* self, Node* node) {
    if (node->return_.right) {
        resolver_node(self, node->return_.right, false);
    }
    node->return_.procedure = self->current_procedure;
}

void resolver_if_branch(Resolver* self, Node* node) {
    if (node->if_branch.cond) {
        resolver_node(self, node->if_branch.cond, false);
    }
    resolver_node(self, node->if_branch.body, false);
}

void resolver_if_expr(Resolver* self, Node* node) {
    resolver_if_branch(self, node->if_expr.if_branch);
    buf_loop(node->if_expr.else_if_branch, i) {
        resolver_if_branch(self, node->if_expr.else_if_branch[i]);
    }
    if (node->if_expr.else_branch) {
        resolver_if_branch(self, node->if_expr.else_branch);
    }
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
    self->current_procedure = node;
    resolver_create_scope(self, scope);
    buf_loop(node->procedure_decl.params, i) {
        resolver_node(self, node->procedure_decl.params[i], false);
    }

    resolver_type(self, node->procedure_decl.type);
    resolver_block(self, node->procedure_decl.body, false);

    resolver_revert_scope(self, scope);
    self->current_procedure = null;
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
// order-independant declarations.
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
        case NODE_KIND_IMPLICIT_MODULE:
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
        case NODE_KIND_RETURN:
        case NODE_KIND_BLOCK:
        case NODE_KIND_IF_EXPR:
        {
            // DO NOTHING
        } break;

        case NODE_KIND_NUMBER:
        case NODE_KIND_CONSTANT:
        case NODE_KIND_TYPE_PRIMITIVE:
        case NODE_KIND_TYPE_CUSTOM:
        case NODE_KIND_TYPE_PTR:
        case NODE_KIND_SYMBOL:
        case NODE_KIND_UNARY:
        case NODE_KIND_DEREF:
        case NODE_KIND_CAST:
        case NODE_KIND_BINARY:
        case NODE_KIND_ASSIGN:
        case NODE_KIND_PROCEDURE_CALL:
        {
            assert(0);
        } break;
    }
}

void resolver_type_custom(Resolver* self, Node* node) {
    Token* identifier = node->type_custom.symbol->symbol.identifier;
    // TODO: check
    error_node(
            node,
            "unresolved type `%s`",
            identifier->lexeme);
}

void resolver_type_ptr(Resolver* self, Node* node) {
    resolver_type(self, node->type_ptr.right);
}

void resolver_type(Resolver* self, Node* node) {
    switch (node->kind) {
        case NODE_KIND_TYPE_PRIMITIVE: break;

        case NODE_KIND_TYPE_CUSTOM:
        {
            resolver_type_custom(self, node);
        } break;

        case NODE_KIND_TYPE_PTR:
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

        case NODE_KIND_RETURN:
        {
            resolver_return(self, node);
        } break;

        case NODE_KIND_PROCEDURE_CALL:
        {
            resolver_procedure_call(self, node);
        } break;
        
        case NODE_KIND_SYMBOL:
        {
            resolver_symbol(self, node);
        } break;

        case NODE_KIND_UNARY:
        {
            resolver_expr_unary(self, node);
        } break;

        case NODE_KIND_DEREF:
        {
            resolver_expr_deref(self, node);
        } break;

        case NODE_KIND_CAST:
        {
            resolver_expr_cast(self, node);
        } break;

        case NODE_KIND_BINARY:
        {
            resolver_expr_binary(self, node);
        } break;

        case NODE_KIND_ASSIGN:
        {
            resolver_expr_assign(self, node);
        } break;

        case NODE_KIND_BLOCK:
        {
            resolver_block(self, node, true);
        } break;

        case NODE_KIND_IF_EXPR:
        {
            resolver_if_expr(self, node);
        } break;

        case NODE_KIND_TYPE_PRIMITIVE:
		case NODE_KIND_TYPE_CUSTOM:
		case NODE_KIND_TYPE_PTR:
        {
            resolver_type(self, node);
        } break;

        case NODE_KIND_NUMBER:
        {
        } break;

        case NODE_KIND_CONSTANT:
        {
        } break;
    }
}

void resolver_init(Resolver* self, SrcFile* srcfile) {
    self->srcfile = srcfile;
    self->global_scope = scope_from_scope(null);
    self->current_scope = self->global_scope;
    self->error = false;
    self->current_procedure = null;
}

void resolver_resolve(Resolver* self) {
    buf_loop(self->srcfile->nodes, i) {
        resolver_pre_decl_node(self, self->srcfile->nodes[i], false);
    }

    buf_loop(self->srcfile->nodes, i) {
        resolver_node(self, self->srcfile->nodes[i], true);
    }
}

