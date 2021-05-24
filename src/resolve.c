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

typedef struct {
    SrcFile* srcfile;
    Scope* global_scope;
    Scope* current_scope;
    bool error;
} Resolver;

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

void resolver_init(Resolver* self, SrcFile* srcfile) {
    self->srcfile = srcfile;
    self->global_scope = scope_from_scope(null);
    self->current_scope = self->global_scope;
    self->error = false;
}

void resolver_resolve(Resolver* self) {
    buf_loop(self->srcfile->nodes, i) {
        resolver_cpush_scope(self, self->srcfile->nodes[i]);
    }
}

