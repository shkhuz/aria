typedef struct {
    SrcFile* srcfile;
    bool error;
} Analyzer;

Node* analyzer_expr(Analyzer* self, Node* node);
void analyzer_stmt(Analyzer* self, Node* node);

Node* analyzer_number(Analyzer* self, Node* node) {
    return u64_type;
}

Node* analyzer_symbol(Analyzer* self, Node* node) {
    assert(node->symbol.ref);
    return node_get_type(node, true);
}

Node* analyzer_procedure_call(Analyzer* self, Node* node) {
    return analyzer_expr(self, node->procedure_call.callee);
}

Node* analyzer_block(Analyzer* self, Node* node) {
    buf_loop(node->block.nodes, i) {
        analyzer_stmt(self, node->block.nodes[i]);
    }
    return null;
}

Node* analyzer_expr(Analyzer* self, Node* node) {
    switch (node->kind) {
        case NODE_KIND_NUMBER:
            return analyzer_number(self, node);
        case NODE_KIND_SYMBOL:
            return analyzer_symbol(self, node);
        case NODE_KIND_PROCEDURE_CALL:
            return analyzer_procedure_call(self, node);

        case NODE_KIND_VARIABLE_DECL:
        case NODE_KIND_PROCEDURE_DECL:
        case NODE_KIND_EXPR_STMT:
        {
            assert(0);
        } break;
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

void analyzer_procedure_decl(Analyzer* self, Node* node) {
    analyzer_block(self, node->procedure_decl.body);
}

void analyzer_stmt(Analyzer* self, Node* node) {
    switch (node->kind) {
        case NODE_KIND_VARIABLE_DECL:
        {
            analyzer_variable_decl(self, node);
        } break;

        case NODE_KIND_PROCEDURE_DECL:
        {
            analyzer_procedure_decl(self, node);
        } break;

        case NODE_KIND_EXPR_STMT: 
        {
            analyzer_expr(self, node->expr_stmt.expr);
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

