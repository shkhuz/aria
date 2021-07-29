typedef struct {
    SrcFile* srcfile;
    bool error;
    int local_vars_bytes;
} Analyzer;

Node* analyzer_expr(Analyzer* self, Node* node, Node* cast_to_type);
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
                primitive_type_size(to->type_primitive.kind) ||
                primitive_type_is_signed(from->type_primitive.kind) != 
                primitive_type_is_signed(to->type_primitive.kind)) {
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

bool analyzer_bigint_fits(bigint* a, TypePrimitiveKind kind) {
    switch (kind) {
        case TYPE_PRIMITIVE_KIND_U8:
            return bigint_fits_u8(a);
        case TYPE_PRIMITIVE_KIND_U16:
            return bigint_fits_u16(a);
        case TYPE_PRIMITIVE_KIND_U32:
            return bigint_fits_u32(a);
        case TYPE_PRIMITIVE_KIND_U64:
            return bigint_fits_u64(a);
        case TYPE_PRIMITIVE_KIND_USIZE:
            return bigint_fits_u64(a);

        case TYPE_PRIMITIVE_KIND_I8:
            return bigint_fits_i8(a);
        case TYPE_PRIMITIVE_KIND_I16:
            return bigint_fits_i16(a);
        case TYPE_PRIMITIVE_KIND_I32:
            return bigint_fits_i32(a);
        case TYPE_PRIMITIVE_KIND_I64:
            return bigint_fits_i64(a);
        case TYPE_PRIMITIVE_KIND_ISIZE:
            return bigint_fits_i64(a);

        case TYPE_PRIMITIVE_KIND_VOID: 
        case TYPE_PRIMITIVE_KIND_NONE: assert(0); break;
    }
    return false;
}

Node* analyzer_get_largest_type_for_bigint(
        Analyzer* self, 
        bigint* val, 
        Node* node,
        Node* cast_to_type) {
    if (cast_to_type && 
        cast_to_type->kind == NODE_KIND_TYPE_PRIMITIVE &&
        analyzer_bigint_fits(val, cast_to_type->type_primitive.kind)) {
        return cast_to_type;
    } else if (cast_to_type && 
        cast_to_type->kind == NODE_KIND_TYPE_PRIMITIVE) {
        char* cast_to_type_str = type_to_str(cast_to_type);
        error_node(
                node,
                "cannot convert to `" ANSI_FCYAN "%s" ANSI_RESET "`",
                cast_to_type_str);
        return null;
        buf_free(cast_to_type);
    }

    if (bigint_fits_u8(val)) {
        return primitive_type_placeholders.u8;
    } else if (bigint_fits_i8(val)) {
        return primitive_type_placeholders.i8;
    } else if (bigint_fits_u16(val)) {
        return primitive_type_placeholders.u16;
    } else if (bigint_fits_i16(val)) {
        return primitive_type_placeholders.i16;
    } else if (bigint_fits_u32(val)) {
        return primitive_type_placeholders.u32;
    } else if (bigint_fits_i32(val)) {
        return primitive_type_placeholders.i32;
    } else if (bigint_fits_u64(val)) {
        return primitive_type_placeholders.u64;
    } else if (bigint_fits_i64(val)) {
        return primitive_type_placeholders.i64;
    } else {
        error_node(
                node,
                "integer overflow");
    }
    return null;
}

Node* analyzer_number(Analyzer* self, Node* node, Node* cast_to_type) {
    /* switch (node->number.number->kind) { */
    /*     case TOKEN_KIND_NUMBER_8: */
    /*     case TOKEN_KIND_NUMBER_16: */
    /*     case TOKEN_KIND_NUMBER_32: */
    /*         return primitive_type_placeholders.u32; */
    /*     case TOKEN_KIND_NUMBER_64: */
    /*         return primitive_type_placeholders.u64; */
    /*     default: */
    /*     { */
    /*         assert(0); */
    /*     } break; */
    /* } */
    return analyzer_get_largest_type_for_bigint(
            self, 
            node->number.val, 
            node, 
            cast_to_type);
}

Node* analyzer_symbol(Analyzer* self, Node* node) {
    assert(node->symbol.ref);
    return node_get_type(node, true);
}

Node* analyzer_expr_unary(Analyzer* self, Node* node, Node* cast_to_type) {
    if (node->unary.op->kind == TOKEN_KIND_MINUS) {
        const bigint* val = null;
        Node* right_type = analyzer_expr(self, node->unary.right, null);
        if (right_type && type_is_integer(right_type)) {
            if ((val = node_get_val_number(node->unary.right))) {
                alloc_with_type_no_decl(node->unary.val, bigint);
                bigint_init(node->unary.val);
                bigint_neg(val, node->unary.val);
                return analyzer_get_largest_type_for_bigint(
                        self, 
                        node->unary.val, 
                        node,
                        cast_to_type);
            } else {
                return analyzer_expr(self, node->unary.right, cast_to_type);
            }
        } else {
            if (right_type) {
                error_node(
                        node->unary.right,
                        "unary (-) operator requires an integer");
            }
        }
    }
    return null;
}

Node* analyzer_expr_binary(Analyzer* self, Node* node, Node* cast_to_type) {
    if (node->binary.op->kind == TOKEN_KIND_PLUS ||
        node->binary.op->kind == TOKEN_KIND_MINUS) {
        const bigint* left_val = null;
        const bigint* right_val = null;
        Node* left_type = analyzer_expr(self, node->binary.left, null);
        Node* right_type = analyzer_expr(self, node->binary.right, null);
        if (left_type && right_type) {
            if (type_is_integer(left_type) && type_is_integer(right_type)) {
                if (primitive_type_is_signed(left_type->type_primitive.kind) ==
                    primitive_type_is_signed(right_type->type_primitive.kind)) {
                    if ((left_val = node_get_val_number(node->binary.left)) &&
                        (right_val = node_get_val_number(node->binary.right))) {
                        alloc_with_type_no_decl(node->binary.val, bigint);
                        bigint_init(node->binary.val);
                        if (node->binary.op->kind == TOKEN_KIND_PLUS) {
                            bigint_add(left_val, right_val, node->binary.val);
                        } else if (node->binary.op->kind == TOKEN_KIND_MINUS) {
                            bigint_sub(left_val, right_val, node->binary.val);
                        } else assert(0);
                        return analyzer_get_largest_type_for_bigint(
                                self, 
                                node->binary.val, 
                                node,
                                cast_to_type);
                    }
                } else {
                    error_token(
                            node->binary.op,
                            "operator requires arguments to be of "
                            "same signedness");
                }
            } else {
                error_token(
                        node->binary.op,
                        "operator requires an integer "
                        "or a pointer argument");
            } 
        }
    }
    return null;
}

Node* analyzer_expr_assign(Analyzer* self, Node* node) {
    Node* left_type = analyzer_expr(
            self, 
            node->assign.left,
            null);
    Node* right_type = analyzer_expr(
            self, 
            node->assign.right,
            null);

    if (left_type && right_type && 
            !implicit_cast(
                right_type, 
                left_type)) {
        error_type_mismatch_node(
                node,
                right_type,
                left_type);
    }
    return null;
}

Node* analyzer_procedure_call(Analyzer* self, Node* node) {
    Node** args = node->procedure_call.args;
    u64 arg_len = buf_len(args);
    Node** params = 
        node->procedure_call.callee->symbol.ref->procedure_decl.params;
    u64 param_len = buf_len(params);
    Node* return_type = analyzer_expr(self, node->procedure_call.callee, null);

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
        Node* param_type = params[i]->param.type;
        Node* arg_type = analyzer_expr(self, args[i], param_type);
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

Node* analyzer_expr(Analyzer* self, Node* node, Node* cast_to_type) {
    switch (node->kind) {
        case NODE_KIND_NUMBER:
            return analyzer_number(self, node, cast_to_type);
        case NODE_KIND_SYMBOL:
            return analyzer_symbol(self, node);
        case NODE_KIND_UNARY:
            return analyzer_expr_unary(self, node, cast_to_type);
        case NODE_KIND_BINARY:
            return analyzer_expr_binary(self, node, cast_to_type);
        case NODE_KIND_ASSIGN:
            return analyzer_expr_assign(self, node);
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
        Node* initializer_type = analyzer_expr(
                self, 
                node->variable_decl.initializer, 
                annotated_type);

        if (annotated_type && initializer_type && 
                !implicit_cast(
                    annotated_type, 
                    initializer_type)) {
            error_type_mismatch_node(
                    node->variable_decl.initializer,
                    initializer_type,
                    annotated_type);
        }
    } else if (node->variable_decl.initializer) {
        node->variable_decl.type = 
            analyzer_expr(self, node->variable_decl.initializer, null);
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
            analyzer_expr(self, node->expr_stmt.expr, null);
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

