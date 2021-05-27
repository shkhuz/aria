typedef struct {
    SrcFile* srcfile;
    bool error;
} Analyzer;

Node* analyzer_expr(Analyzer* self, Node* node);
void analyzer_stmt(Analyzer* self, Node* node);

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

Node* analyzer_number(Analyzer* self, Node* node) {
    return u64_type;
}

Node* analyzer_symbol(Analyzer* self, Node* node) {
    assert(node->symbol.ref);
    return node_get_type(node, true);
}

Node* analyzer_procedure_call(Analyzer* self, Node* node) {
    Node** args = node->procedure_call.args;
    u64 arg_len = buf_len(args);
    Node** params = 
        node->procedure_call.callee->symbol.ref->procedure_decl.params;
    u64 param_len = buf_len(params);
    Node* return_type = analyzer_expr(self, node->procedure_call.callee);

    if (arg_len != param_len) {
        if (arg_len < param_len) {
            error_expect_type_token(
                    node->tail,
                    params[arg_len]->param.type);
            note_node(
                    params[arg_len],
                    "...parameter declared here");
        } else {
            error_node(
                    args[param_len],
                    "extra argument(s) supplied");
        }
        return return_type;
    }

    buf_loop(args, i) {
        Node* arg_type = analyzer_expr(self, args[i]);
        Node* param_type = params[i]->param.type;
        if (arg_type && param_type && 
                !implicit_cast(
                    arg_type, 
                    param_type)) {
            error_type_mismatch_node(
                    args[i],
                    arg_type,
                    param_type);
            note_node(
                    param_type,
                    "...parameter type annotated here");
        }
    }

    return return_type;
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

void analyzer_variable_decl(Analyzer* self, Node* node) {
    if (node->variable_decl.type && node->variable_decl.initializer) {
        Node* annotated_type = node->variable_decl.type;
        Node* initializer_type = 
            analyzer_expr(self, node->variable_decl.initializer);

        if (annotated_type && initializer_type && 
                !implicit_cast(
                    annotated_type, 
                    initializer_type)) {
            error_type_mismatch_node(
                    node->variable_decl.initializer,
                    initializer_type,
                    annotated_type);
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

