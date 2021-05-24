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

// Pushes a node into the scope symbol table 
// if not already pushed.
// If redeclaration occurs, raises an error and 
// returns.
void resolver_cpush_scope(Resolver* self, Node* node) {
    Token* identifier = node_get_identifier(node, true);
    assert(identifier);

    buf_loop(self->current_scope->nodes, i) {
        Token* scope_identifier = node_get_identifier(
                self->current_scope->nodes[i],
                true);
        assert(scope_identifier);

        if (token_lexeme_eq(identifier, scope_identifier)) {
            error_node(
                    node,
                    "redeclaration of symbol `%s`",
                    identifier->lexeme);
            note_node(
                    self->current_scope->nodes[i],
                    "...previously declared here");
            return;
        }
    }

    buf_push(self->current_scope->nodes, node);
}

void resolver_block(Resolver* self, Node* node) {
    buf_loop(node->block.nodes, i) {
        resolver_pre_decl_node(self, node->block.nodes[i], true);
    }

    buf_loop(node->block.nodes, i) {
        resolver_node(self, node->block.nodes[i], false);
    }
}

void resolver_variable_decl(
        Resolver* self, 
        Node* node,
        bool ignore_procedure_level_decl_node_pre_check) {
    if (!ignore_procedure_level_decl_node_pre_check) {
        resolver_cpush_scope(self, node);
    }

    /* resolver_type(self, node->variable_decl.type); */
}

void resolver_procedure_decl(Resolver* self, Node* node) {
    resolver_create_scope(self, scope);
    resolver_block(self, node->procedure_decl.body);
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
            resolver_cpush_scope(self, node);
        } break;

        case NODE_KIND_VARIABLE_DECL:
        {
            if (!ignore_procedure_level_decl_node) {
                resolver_cpush_scope(self, node);
            }
        } break;

        case NODE_KIND_EXPR_STMT:
        case NODE_KIND_BLOCK:
        {
            // DO NOTHING
        } break;

        case NODE_KIND_TYPE:
        case NODE_KIND_SYMBOL:
        case NODE_KIND_PROCEDURE_CALL:
        {
            assert(0);
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

