typedef struct {
    SrcFile* srcfile;
    bool error;
} Analyzer;

Node* analyzer_number(Analyzer* self, Node* node) {
    return u64_type;
}

Node* analyzer_expr(Analyzer* self, Node* node) {
    switch (node->kind) {
        case NODE_KIND_NUMBER:
            return analyzer_number(self, node);
    }
    assert(0);
    return null;
}

Node* get_inner_type(Node* type) {
    assert(type->kind == NODE_KIND_TYPE);
    switch (type->type.kind) {
        case TYPE_KIND_BASE: return null;
        case TYPE_KIND_PTR: return type->type.ptr.right;
    }
    assert(0);
    return null;
}

bool implicit_cast(Node* a, Node* b) {
    assert(a && b);
    assert(a->kind == NODE_KIND_TYPE && b->kind == NODE_KIND_TYPE);

    if (a->type.kind == b->type.kind) {
        if (a->type.kind == TYPE_KIND_BASE) {
            if (token_lexeme_eq(
                        a->type.base.symbol->symbol.identifier,
                        b->type.base.symbol->symbol.identifier)) {
                return true;
            }
        } else {
            return implicit_cast(
                    get_inner_type(a),
                    get_inner_type(b));
        }
    } 

    return false;
}

void analyzer_variable_decl(Analyzer* self, Node* node) {
    if (node->variable_decl.type && node->variable_decl.initializer) {
        Node* annotated_type = node->variable_decl.type;
        Node* initializer_type = 
            analyzer_expr(self, node->variable_decl.initializer);

        if (annotated_type && initializer_type) {
            if (!implicit_cast(
                        annotated_type, 
                        initializer_type)) {
                error_type_mismatch_node(
                        node->variable_decl.initializer,
                        initializer_type,
                        annotated_type);
            }
        }
    }
}

void analyzer_stmt(Analyzer* self, Node* node) {
    switch (node->kind) {
        case NODE_KIND_VARIABLE_DECL:
        {
            analyzer_variable_decl(self, node);
        } break;
    }
}

void analyzer_init(Analyzer* self, SrcFile* srcfile) {
    self->srcfile = srcfile;
    self->error = false;
}

void analyzer_analyze(Analyzer* self) {
    buf_loop(self->srcfile->nodes, i) {
        analyzer_stmt(self, self->srcfile->nodes[i]);
    }
}

