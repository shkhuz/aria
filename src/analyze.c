typedef struct {
    SrcFile* srcfile;
    bool error;
    int local_vars_bytes;
} Analyzer;

Node* analyzer_expr(Analyzer* self, Node* node);
void analyzer_stmt(Analyzer* self, Node* node);

Node* get_inner_type(Node* type) {
    switch (type->kind) {
        case NODE_KIND_TYPE_CUSTOM:
        case NODE_KIND_TYPE_PRIMITIVE: return null;
        case NODE_KIND_TYPE_PTR: return type->type_ptr.right;
    }
    assert(0);
    return null;
}

bool implicit_cast(Node* from, Node* to) {
    assert(from && to);
    //assert(from->kind == NODE_KIND_TYPE && to->kind == NODE_KIND_TYPE);

    if (from->kind == to->kind) {
        if (from->kind == NODE_KIND_TYPE_CUSTOM) {
            if (token_lexeme_eq(
                        from->type_custom.symbol->symbol.identifier,
                        to->type_custom.symbol->symbol.identifier)) {
                return true;
            }
        } else if (from->kind == NODE_KIND_TYPE_PRIMITIVE) {
            if (primitive_type_size(from->type_primitive.kind) >
                primitive_type_size(to->type_primitive.kind)) {
                return false;
            }
            return true;
        } else {
            return implicit_cast(
                    get_inner_type(from),
                    get_inner_type(to));
        }
    } 

    return false;
}

Node* analyzer_number(Analyzer* self, Node* node) {
    switch (node->number.number->kind) {
        case TOKEN_KIND_NUMBER_8:
            return primitive_type_placeholders.u8;
        case TOKEN_KIND_NUMBER_16:
            return primitive_type_placeholders.u16;
        case TOKEN_KIND_NUMBER_32:
            return primitive_type_placeholders.u32;
        case TOKEN_KIND_NUMBER_64:
            return primitive_type_placeholders.u64;
        default:
        {
            assert(0);
        } break;
    }
    return null;
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
        case NODE_KIND_BLOCK: 
            return null;

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

    if (node->variable_decl.initializer) {
        node->variable_decl.type = 
            analyzer_expr(self, node->variable_decl.initializer);
        if (!node->variable_decl.type) return;
    }

    if (node->variable_decl.in_procedure) {
        self->local_vars_bytes += 
            type_get_size_bytes(node->variable_decl.type);
    }
}

void analyzer_procedure_decl(Analyzer* self, Node* node) {
    self->local_vars_bytes = 0;
    analyzer_block(self, node->procedure_decl.body);
    node->procedure_decl.local_vars_bytes = self->local_vars_bytes;
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
    self->local_vars_bytes = 0;
}

void analyzer_analyze(Analyzer* self) {
    buf_loop(self->srcfile->nodes, i) {
        analyzer_stmt(self, self->srcfile->nodes[i]);
    }
}

