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
            if (primitive_type_is_integer(from->type_primitive.kind) &&
                primitive_type_is_integer(to->type_primitive.kind)) {
                if (primitive_type_size(from->type_primitive.kind) >
                    primitive_type_size(to->type_primitive.kind) ||
                    primitive_type_is_signed(from->type_primitive.kind) != 
                    primitive_type_is_signed(to->type_primitive.kind)) {
                    return false;
                } else return true;
            } else {
                if (from->type_primitive.kind == TYPE_PRIMITIVE_KIND_VOID &&
                    to->type_primitive.kind == TYPE_PRIMITIVE_KIND_VOID) {
                    return true;
                } else if (from->type_primitive.kind == TYPE_PRIMITIVE_KIND_BOOL 
                        && to->type_primitive.kind == TYPE_PRIMITIVE_KIND_BOOL) {
                    return true;
                } else return false;
            }
        } else if (from->kind == NODE_KIND_TYPE_PTR) {
            // There are four cases:
            //           to           C = const
            //      |---------|       M = mutable
            //      |   | C M |       O = okay to cast
            //      |---------|       ! = cannot cast
            // from | C | O ! |
            //      | M | O O |
            //      |---------|
            // 
            // These four cases are handled by this one line
            if (!from->type_ptr.constant || to->type_ptr.constant) {
                return implicit_cast(
                        get_inner_type(from),
                        get_inner_type(to));
            }
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
        type_is_integer(cast_to_type) &&
        analyzer_bigint_fits(val, cast_to_type->type_primitive.kind)) {
        return cast_to_type;
    } else if (cast_to_type && 
               type_is_integer(cast_to_type)) {
        error_cannot_convert_to_node(
                node,
                cast_to_type);
        return null;
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

Node* analyzer_boolean(Analyzer* self, Node* node, Node* cast_to_type) {
    return primitive_type_placeholders.bool_;
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
                        "unary `-` operator requires an integer");
                note_root_type_single(right_type);
            }
        }
    } else if (node->unary.op->kind == TOKEN_KIND_AMPERSAND) {
        Node* right_type = analyzer_expr(self, node->unary.right, null);
        if (right_type) {
            return node_type_ptr_new(
                    null, 
                    node->unary.right->symbol.ref->variable_decl.constant,
                    right_type);
        }
    }
    return null;
}

Node* analyzer_expr_deref(Analyzer* self, Node* node) {
    Node* right_type = analyzer_expr(self, node->deref.right, null);
    if (right_type) {
        if (right_type->kind == NODE_KIND_TYPE_PTR) {
            node->deref.constant = right_type->type_ptr.constant;
            return right_type->type_ptr.right;
        } else {
            error_node(
                    node->deref.right,
                    "cannot dereference a non-pointer type");
            note_root_type_single(right_type);
        }
    }
    return null;
}

Node* analyzer_expr_cast(Analyzer* self, Node* node) {
    Node* left_type = analyzer_expr(self, node->cast.left, null);
    Node* cast_to = node->cast.right;
    if (type_is_void(left_type) && type_is_void(cast_to)) {
            return cast_to;
    } else if (type_is_void(left_type) || type_is_void(cast_to)) {
        error_type_mismatch_node(
                node->cast.left,
                left_type,
                cast_to);
        return null;
    }
    return cast_to;
}

Node* analyzer_expr_binary(Analyzer* self, Node* node, Node* cast_to_type) {
    if (node->binary.op->kind == TOKEN_KIND_PLUS ||
        node->binary.op->kind == TOKEN_KIND_MINUS) {
        const bigint* left_val = null;
        const bigint* right_val = null;
        Node* left_type = analyzer_expr(self, node->binary.left, null);
        Node* right_type = analyzer_expr(self, node->binary.right, left_type);
        bool checked_reversed = false;
        if (left_type && right_type) {
            if (type_is_integer(left_type) && type_is_integer(right_type)) {
check_signedness:
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
                    } else {
                        if (type_get_size_bytes(left_type) >= 
                            type_get_size_bytes(right_type)) {
                            return left_type;
                        } else return right_type;
                    }
                } else {
                    if (!checked_reversed) {
                        checked_reversed = true;
                        g_block_error_msgs = true;
                        right_type = analyzer_expr(
                                self, 
                                node->binary.right, 
                                null);
                        left_type = analyzer_expr(
                                self, 
                                node->binary.left, 
                                right_type);
                        g_block_error_msgs = false;
                        goto check_signedness;
                    }
                    error_token(
                            node->binary.op,
                            "operator requires arguments to be of "
                            "same signedness");
                    note_root_type_double(
                            left_type,
                            right_type);
                }
            } else if ((left_type->kind == NODE_KIND_TYPE_PTR && 
                       type_is_integer(right_type)) ||
                       (right_type->kind == NODE_KIND_TYPE_PTR && 
                       type_is_integer(left_type))) {
                if (right_type->kind == NODE_KIND_TYPE_PTR && 
                       type_is_integer(left_type)) {
                    // Canonicalize ptr-arithmetic
                    Node* tmp = left_type;
                    left_type = right_type;
                    right_type = tmp;
                }
                return left_type;
            } else {
                error_token(
                        node->binary.op,
                        "operator requires an integer "
                        "or a pointer argument");
                note_root_type_double(
                        left_type,
                        right_type);
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
            left_type);
    bool checked = false;

check:
    if (left_type && right_type && 
            !implicit_cast(
                right_type, 
                left_type)) {
        if (!checked) {
            checked = true;
            right_type = analyzer_expr(
                    self, 
                    node->assign.right,
                    null);
            left_type = analyzer_expr(
                    self, 
                    node->assign.left,
                    right_type);
            goto check;
        }
        error_type_mismatch_token(
                node->assign.op,
                right_type,
                left_type);
        return null;
    }

    if (node->assign.left->kind == NODE_KIND_DEREF) {
        if (node->assign.left->deref.constant) {
            error_node(
                    node->assign.left,
                    "cannot modify contents of a const-pointer");
        }
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

Node* analyzer_block(Analyzer* self, Node* node, Node* cast_to_type) {
    buf_loop(node->block.nodes, i) {
        analyzer_stmt(self, node->block.nodes[i]);
    }
    if (node->block.return_value) {
        return analyzer_expr(self, node->block.return_value, cast_to_type);
    } else {
        return primitive_type_placeholders.void_;
    }
}

Node* analyzer_expr(Analyzer* self, Node* node, Node* cast_to_type) {
    switch (node->kind) {
        case NODE_KIND_NUMBER:
            return analyzer_number(self, node, cast_to_type);
        case NODE_KIND_BOOLEAN:
            return analyzer_boolean(self, node, cast_to_type);
        case NODE_KIND_SYMBOL:
            return analyzer_symbol(self, node);
        case NODE_KIND_UNARY:
            return analyzer_expr_unary(self, node, cast_to_type);
        case NODE_KIND_DEREF:
            return analyzer_expr_deref(self, node);
        case NODE_KIND_CAST:
            return analyzer_expr_cast(self, node);
        case NODE_KIND_BINARY:
            return analyzer_expr_binary(self, node, cast_to_type);
        case NODE_KIND_ASSIGN:
            return analyzer_expr_assign(self, node);
        case NODE_KIND_PROCEDURE_CALL:
            return analyzer_procedure_call(self, node);
        case NODE_KIND_BLOCK: 
            return analyzer_block(self, node, cast_to_type);

        case NODE_KIND_TYPE_PRIMITIVE:
        case NODE_KIND_TYPE_CUSTOM:
        case NODE_KIND_TYPE_PTR:
        {
            error_node(
                    node,
                    "type is not expected here");
        } return null;

        case NODE_KIND_VARIABLE_DECL:
        case NODE_KIND_PROCEDURE_DECL:
        case NODE_KIND_EXPR_STMT:
        case NODE_KIND_RETURN:
        {
            assert(0);
        } break;
    }
    assert(0);
    return null;
}

void analyzer_print_note_for_procedure_return_type(Node* procedure) {
    if (procedure->procedure_decl.type->kind == NODE_KIND_TYPE_PRIMITIVE &&
        procedure->procedure_decl.type->type_primitive.token == null) {
        note_token(
                procedure->procedure_decl.body->head,
                "...return type implicitly inferred here");
    } else {
        note_node(
                procedure->procedure_decl.type,
                "...return type annotated here");
    }
}

void analyzer_return(Analyzer* self, Node* node) {
    Node* proc_return_type = node->return_.procedure->procedure_decl.type;
    Node* right_type = primitive_type_placeholders.void_;
    if (node->return_.right) {
        right_type = analyzer_expr(
            self, 
            node->return_.right,
            null);
    }

    if (proc_return_type && right_type &&
            !implicit_cast(
                right_type, 
                proc_return_type)) {
        error_type_mismatch_node(
                node->return_.right,
                right_type,
                proc_return_type);
        analyzer_print_note_for_procedure_return_type(
                node->return_.procedure);
    }
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
                    initializer_type, 
                    annotated_type)) {
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
    Node* annotated_type = node->procedure_decl.type;
    Node* body_type = analyzer_block(
            self, node->procedure_decl.body, 
            annotated_type);
    if (body_type && annotated_type &&
            !implicit_cast(
                body_type,
                annotated_type)) {
        error_type_mismatch_node(
                node->procedure_decl.body->block.return_value,
                body_type,
                annotated_type);
        analyzer_print_note_for_procedure_return_type(node);
    }

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

        case NODE_KIND_RETURN:
        {
            analyzer_return(self, node);
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

