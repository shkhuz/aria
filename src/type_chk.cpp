typedef struct {
    Srcfile* srcfile;
    bool error;
    size_t last_stack_offset;
} CheckContext;

typedef enum {
    IMPLICIT_CAST_OK,
    IMPLICIT_CAST_ERROR,
} ImplicitCastStatus;

#define check_error(token, fmt, ...) \
    error(token, fmt, ##__VA_ARGS__); \
    c->error = true

ImplicitCastStatus check_implicit_cast(
        CheckContext* c, 
        Type* from, 
        Type* to) {

    assert(from && to);
    if (from->kind == to->kind) {
        if (from->kind == TYPE_KIND_BUILTIN) {
            if (builtin_type_is_integer(from->builtin.kind) &&
                builtin_type_is_integer(to->builtin.kind)) {
                if (builtin_type_bytes(&from->builtin) > 
                    builtin_type_bytes(&to->builtin) ||
                    builtin_type_is_signed(from->builtin.kind) != 
                    builtin_type_is_signed(to->builtin.kind)) {
                    return IMPLICIT_CAST_ERROR;
                }
                else {
                    return IMPLICIT_CAST_OK;
                }
            }
            else if (builtin_type_is_apint(from->builtin.kind) || 
                     builtin_type_is_apint(to->builtin.kind)) {
                if (builtin_type_is_apint(to->builtin.kind)) {
                    // from always points to apint
                    SWAP_VARS(Type*, from, to);
                }

                if (!type_is_integer(to)) {
                    return IMPLICIT_CAST_ERROR;
                }
                else if (bigint_fits(
                            &from->builtin.apint,
                            builtin_type_bytes(&to->builtin),
                            builtin_type_is_signed(to->builtin.kind))) {
                    from->builtin.kind = to->builtin.kind;
                    return IMPLICIT_CAST_OK;
                }
                else {
                    return IMPLICIT_CAST_ERROR;
                }
            }
            else if (from->builtin.kind == to->builtin.kind) {
                if (from->builtin.kind == BUILTIN_TYPE_KIND_BOOLEAN ||
                    from->builtin.kind == BUILTIN_TYPE_KIND_VOID) {
                    return IMPLICIT_CAST_OK;
                } 
                else {
                    return IMPLICIT_CAST_ERROR;
                }
            }
        }
        else if (from->kind == TYPE_KIND_PTR) {
            // There are four cases:
            //           to           C = const
            //      +---+-----+       M = mutable
            //      |   | C M |       O = okay to cast
            //      +---+-----+       ! = cannot cast
            // from | C | O ! |
            //      | M | O O |
            //      +---+-----+
            // 
            // These four cases are handled by this one line:
            if (!from->ptr.constant || to->ptr.constant) {
                return check_implicit_cast(
                        c,
                        type_get_child(from),
                        type_get_child(to));
            }
        }
    }
    return IMPLICIT_CAST_ERROR;
}

bool check_apint_int_operands(
        CheckContext* c,
        Expr* expr,
        Type* left_type,
        Type* right_type,
        bool is_left_apint,
        bool is_right_apint) {
    Type* apint_type = left_type;
    Type* int_type = right_type;
    if (is_right_apint) {
        SWAP_VARS(Type*, apint_type, int_type);
    }

    if (!type_is_integer(int_type)) {
        check_error(
                (is_right_apint ? 
                 expr->binop.left->main_token : 
                 expr->binop.right->main_token),
                "expression cannot be converted to `{}`",
                *apint_type);
        return false;
    }

    if (bigint_fits(
                &apint_type->builtin.apint,
                builtin_type_bytes(&int_type->builtin),
                builtin_type_is_signed(int_type->builtin.kind))) {
        expr->type = int_type;
        return true;
    }
    else {
        check_error(
                apint_type->main_token,
                "expression cannot be converted to `{}`",
                *int_type);
        return false;
    }
}

void check_apint_fits_into_default_integer_type(
        CheckContext* c,
        Expr* apint_expr,
        Type* apint) {
    assert(type_is_apint(apint));
    if (!bigint_fits(
                &apint->builtin.apint,
                builtin_type_bytes(&builtin_type_placeholders.int32->builtin),
                builtin_type_is_signed(builtin_type_placeholders.int32->builtin.kind))) {
        check_error(
                apint_expr->main_token,
                "integer cannot be converted to `{}`",
                *builtin_type_placeholders.int32);
    }
}

Type* check_expr(
        CheckContext* c, 
        Expr* expr, 
        Type* cast, 
        bool cast_to_definitive_type);
void check_stmt(CheckContext* c, Stmt* stmt);

Type* check_integer_expr(
        CheckContext* c, 
        Expr* expr, 
        Type* cast, 
        bool cast_to_definitive_type) {
    if (!cast && !cast_to_definitive_type) {
        Type* type = builtin_type_new(
                expr->main_token,
                BUILTIN_TYPE_KIND_APINT);
        type->builtin.apint = *expr->integer.val;
        return type;
    }
    else {
        if (!cast) cast = builtin_type_placeholders.int32;
        if (type_is_integer(cast) && bigint_fits(
                    expr->integer.val, 
                    builtin_type_bytes(&cast->builtin), 
                    builtin_type_is_signed(cast->builtin.kind))) {
            return cast;
        }
        else {
            check_error(
                    expr->integer.integer,
                    "integer cannot be converted to `{}`",
                    *cast);
        }
    }
    return null;
}

Type* check_constant_expr(CheckContext* c, Expr* expr, Type* cast) {
    Type* type = null;
    switch (expr->constant.kind) {
        case CONSTANT_KIND_BOOLEAN_TRUE:
        case CONSTANT_KIND_BOOLEAN_FALSE: {
            type = builtin_type_placeholders.boolean;
        } break;

        case CONSTANT_KIND_NULL: {
            type = builtin_type_placeholders.void_ptr;
        } break;
    }
    expr->type = cast;
    return type;
}

Type* check_symbol_expr(CheckContext* c, Expr* expr, Type* cast) {
    assert(expr->symbol.ref);
    if (expr->symbol.ref->kind != STMT_KIND_FUNCTION) {
        expr->type = cast;
        Type* type = stmt_get_type(expr->symbol.ref);
        return type;
    }
    else {
        check_error(
                expr->main_token, 
                "internal_error: function ptr not implemented");
        return null;
    }
}

Type* check_function_call_expr(CheckContext* c, Expr* expr, Type* cast) {
    std::vector<Expr*>* args = &expr->function_call.args;
    size_t arg_len = args->size();
    assert(expr->function_call.callee->symbol.ref->kind == STMT_KIND_FUNCTION);

    std::vector<Stmt*>* params = &expr->function_call.callee->symbol.ref->function.header->params;
    size_t param_len = params->size();
    Type* callee_return_type = expr->function_call.callee->symbol.ref->function.header->return_type;
    expr->type = cast;

    if (arg_len != param_len) {
        if (arg_len < param_len) {
            std::string fmt = fmt::format(
                    "expected {} additional argument(s) of type ", 
                    param_len - arg_len);
            for (size_t i = arg_len; i < param_len; i++) {
                fmt = fmt + fmt::format("`{}`", *(*params)[i]->param.type);
                if (i+1 < param_len) {
                    fmt = fmt + ", ";
                }
            }
            
            check_error(
                    expr->function_call.rparen,
                    fmt);
            note(
                    (*params)[arg_len]->param.identifier,
                    "...parameter(s) declared here");
        }
        else {
            check_error(
                    (*args)[param_len]->main_token,
                    "extra argument(s) supplied");
        }
        return callee_return_type;
    }

    for (size_t i = 0; i < args->size(); i++) {
        Type* param_type = (*params)[i]->param.type;
        Type* arg_type = check_expr(c, (*args)[i], param_type, true);
        /* args[i]->type = arg_type; */
        if (arg_type && param_type && 
            check_implicit_cast(c, arg_type, param_type) == IMPLICIT_CAST_ERROR) {
            check_error(
                    (*args)[i]->main_token,
                    "argument type `{}` cannot be converted to `{}`",
                    *arg_type,
                    *param_type);
            note(
                    param_type->main_token,
                    "...parameter type annotated here");
        }
    }
    return callee_return_type;
}

Type* check_binop_expr(CheckContext* c, Expr* expr, Type* cast) {
    Type* left_type = check_expr(c, expr->binop.left, null, false);
    expr->binop.left_type = left_type;
    Type* right_type = check_expr(c, expr->binop.right, null, false);
    expr->binop.right_type = right_type;
    if (type_bytes(right_type) > type_bytes(left_type)) {
        expr->type = right_type;
    }
    else {
        expr->type = left_type;
    }

    if (left_type && right_type) {
        bool is_left_apint = type_is_apint(left_type);
        bool is_right_apint = type_is_apint(right_type);
        if (expr->binop.op->kind == TOKEN_KIND_PLUS || 
            expr->binop.op->kind == TOKEN_KIND_MINUS) {
            if (type_is_integer(left_type) && type_is_integer(right_type)) {
                ImplicitCastStatus status;
                bool return_left = true;
                if (type_bytes(right_type) > type_bytes(left_type)) {
                    status = check_implicit_cast(c, left_type, right_type);
                    return_left = false;
                }
                else {
                    status = check_implicit_cast(c, right_type, left_type);
                }
                if (status == IMPLICIT_CAST_ERROR) {
                    check_error(
                            expr->main_token,
                            "implicit cast error: `{}` and `{}`",
                            *left_type,
                            *right_type);
                }
                else if (expr->binop.op->kind == TOKEN_KIND_PLUS ||
                         expr->binop.op->kind == TOKEN_KIND_MINUS) {
                    if (return_left) {
                        return left_type;
                    } else {
                        return right_type;
                    }
                }
            }
            else if (is_left_apint || is_right_apint) {
                if (is_left_apint && is_right_apint) {
                    bigint res;
                    bigint_init(&res);
                    bigint_copy(&left_type->builtin.apint, &res);
                    if (expr->binop.op->kind == TOKEN_KIND_PLUS) {
                        bigint_add(&res, &right_type->builtin.apint, &res);
                    } 
                    else {
                        bigint_sub(&res, &right_type->builtin.apint, &res);
                    }
                    
                    Type* new_apint = builtin_type_new(
                            expr->main_token, 
                            BUILTIN_TYPE_KIND_APINT);
                    new_apint->builtin.apint = res;
                    expr->type = new_apint;
                    return new_apint;
                }
                else {
                    if (check_apint_int_operands(
                            c,
                            expr,
                            left_type,
                            right_type,
                            is_left_apint,
                            is_right_apint)) {
                        return expr->type;
                    }
                    else {
                        return null;
                    }
                }
            }
            else {
                check_error(
                        expr->main_token,
                        "given `{}` and `{}`, but requires integer operands",
                        *left_type,
                        *right_type);
                return null;
            }
        }
        else if (token_is_cmp_op(expr->binop.op)) {
            if (is_left_apint || is_right_apint) {
                if (is_left_apint && is_right_apint) {
                    check_apint_fits_into_default_integer_type(
                            c,
                            expr->binop.left,
                            left_type);
                    check_apint_fits_into_default_integer_type(
                            c,
                            expr->binop.right,
                            right_type);
                }
                else {
                    check_apint_int_operands(
                            c,
                            expr,
                            left_type,
                            right_type,
                            is_left_apint,
                            is_right_apint);
                }
            }
            else {
                ImplicitCastStatus status;
                if (type_bytes(right_type) > type_bytes(left_type)) {
                    status = check_implicit_cast(c, left_type, right_type);
                }
                else {
                    status = check_implicit_cast(c, right_type, left_type);
                }
                if (status == IMPLICIT_CAST_ERROR) {
                    check_error(
                            expr->main_token,
                            "cannot compare `{}` and `{}`",
                            *left_type,
                            *right_type);
                }
            }
            return builtin_type_placeholders.boolean;
        }
    }
    return null;
}

Type* check_unop_expr(CheckContext* c, Expr* expr, Type* cast) {
    Type* child_type = check_expr(c, expr->unop.child, null, false);
    expr->unop.child_type = child_type;
    expr->type = child_type;

    if (child_type) {
        bool is_child_apint = type_is_apint(child_type);
        if (expr->unop.op->kind == TOKEN_KIND_MINUS) {
            if (type_is_integer(child_type)) {
                if (builtin_type_is_signed(child_type->builtin.kind)) {
                    return child_type;
                }
                else {
                    check_error(
                            expr->main_token,
                            "cannot negate unsigned operand of type `{}`",
                            *child_type);
                }
            }
            else if (is_child_apint) {
                bigint res;
                bigint_init(&res);
                bigint_copy(&child_type->builtin.apint, &res);
                bigint_neg(&child_type->builtin.apint, &res);
                
                Type* new_apint = builtin_type_new(
                        expr->main_token, 
                        BUILTIN_TYPE_KIND_APINT);
                new_apint->builtin.apint = res;
                expr->type = new_apint;
                return new_apint;
            }
            else {
                check_error(
                        expr->main_token,
                        "given `{}`, but requires an integer operand",
                        *child_type);
            }
        }
        else if (expr->unop.op->kind == TOKEN_KIND_BANG) {
            if (check_implicit_cast(c, child_type, builtin_type_placeholders.boolean)
                    == IMPLICIT_CAST_ERROR) {
                check_error(
                        expr->main_token,
                        "cannot operate on `{}`",
                        *child_type);
            }
            else {
                return child_type;
            }
        }
        else if (expr->unop.op->kind == TOKEN_KIND_AMP) {
            if (check_implicit_cast(c, child_type, builtin_type_placeholders.void_kind)
                    == IMPLICIT_CAST_ERROR) {
                Stmt* ref = expr->unop.child->symbol.ref;
                bool constant;
                switch (ref->kind) {
                    case STMT_KIND_VARIABLE: { 
                        constant = ref->variable.constant;
                    } break;

                    case STMT_KIND_PARAM: {
                        constant = false;
                    } break;

                    default: {
                        assert(0); 
                    } break;
                }
                return ptr_type_placeholder_new(constant, child_type);
            }
            else {
                check_error(
                        expr->main_token,
                        "address of `{}` types are not valid",
                        *child_type);
            }
        }
        else if (expr->unop.op->kind == TOKEN_KIND_STAR) {
            if (child_type->kind == TYPE_KIND_PTR) {
                Type* ptr_child = child_type->ptr.child;
                if (!type_is_void(ptr_child)) {
                    expr->type = ptr_child;
                    return ptr_child;
                }
                else {
                    check_error(
                            expr->main_token,
                            "dereferencing `{}` is invalid",
                            *child_type);
                }
            }
            else {
                check_error(
                        expr->main_token,
                        "cannot dereference type `{}`",
                        *child_type);
            }
        }
    }
    return null;
}

Type* check_cast_expr(CheckContext* c, Expr* expr, Type* cast) {
    Type* left_type = check_expr(c, expr->cast.left, null, true);
    expr->cast.left_type = left_type;
    Type* to_type = expr->cast.to;
    expr->type = to_type;

    bool is_left_void = type_is_void(left_type);
    bool is_to_void = type_is_void(to_type);

    if (is_left_void && is_to_void) {
        return to_type;
    }
    else if (is_left_void || is_to_void) {
        check_error(
                expr->main_token,
                "cannot cast from `{}` to `{}`",
                *left_type,
                *to_type);
    }
    else if (left_type->kind == TYPE_KIND_PTR && to_type->kind == TYPE_KIND_PTR) {
        if (left_type->ptr.constant && !to_type->ptr.constant) {
            check_error(
                    expr->main_token,
                    "cast to `{}` discards const-ness of `{}`",
                    *to_type,
                    *left_type);
        }
        else {
            return to_type;
        }
    }
    else {
        return to_type;
    }
    return null;
}

Type* check_block_expr(CheckContext* c, Expr* expr, Type* cast) {
    for (Stmt* stmt: expr->block.stmts) {
        check_stmt(c, stmt);
    }
    if (expr->block.value) {
        return check_expr(c, expr->block.value, cast, true);
    }
    return builtin_type_placeholders.void_kind;
}

Type* check_if_branch(CheckContext* c, IfBranch* br, Type* cast) {
    if (br->cond) {
        Type* cond_type = check_expr(c, br->cond, null, true);
        if (cond_type && check_implicit_cast(
                    c, 
                    cond_type, 
                    builtin_type_placeholders.boolean) == IMPLICIT_CAST_ERROR) {
            check_error(
                    br->cond->main_token,
                    "branch condition should be `{}` but got `{}`",
                    *builtin_type_placeholders.boolean,
                    *cond_type);
        }
    }
    return check_block_expr(c, br->body, cast);
}

Type* check_if_expr(CheckContext* c, Expr* expr, Type* cast) {
    // Get branch types
    Type* ifbr_type = check_if_branch(
            c, 
            expr->iff.ifbr,
            cast);
    std::vector<Type*> elseifbr_types;
    for (IfBranch* br: expr->iff.elseifbr) {
        elseifbr_types.push_back(check_if_branch(
                    c,
                    br,
                    cast));
    }
    Type* elsebr_type = null;
    if (expr->iff.elsebr) {
        elsebr_type = check_if_branch(
                c, 
                expr->iff.elsebr,
                cast);
    }

    // Verify branch types
    // TODO: make it so that branch that pass in this "verify" stage are 
    // processed in "compare" stage, even if other branches fail
    if (!ifbr_type) {
        return null;
    }
    for (size_t i = 0; i < expr->iff.elseifbr.size(); i++) {
        if (expr->iff.elseifbr[i] && !elseifbr_types[i]) {
            return null;
        }
    }
    if (expr->iff.elsebr && !elsebr_type) {
        return null;
    }

    if (expr->iff.ifbr->body->block.value && !expr->iff.elsebr) {
        check_error(
                expr->main_token,
                "`else` is mandatory when value is used");
        // TODO: should we return??
    }

    // Compare branch types
    bool err = false;
    for (size_t i = 0; i < elseifbr_types.size(); i++) {
        if (check_implicit_cast(c, elseifbr_types[i], ifbr_type) == IMPLICIT_CAST_ERROR) {
            Token* errtok = null;
            if (expr->iff.elseifbr[i]->body->block.value) {
                errtok = expr->iff.elseifbr[i]->body->block.value->main_token;
            } 
            else {
                errtok = expr->iff.elseifbr[i]->body->block.rbrace;
            }

            check_error(
                    errtok,
                    "`if` returns `{}`, but `else-if` returns `{}`",
                    *ifbr_type,
                    *elseifbr_types[i]);
            err = true;
        }
    }

    if (expr->iff.elsebr) {
        if (check_implicit_cast(c, elsebr_type, ifbr_type) == IMPLICIT_CAST_ERROR) {
            Token* errtok = null;
            if (expr->iff.elsebr->body->block.value) {
                errtok = expr->iff.elsebr->body->block.value->main_token;
            } 
            else {
                errtok = expr->iff.elsebr->body->block.rbrace;
            }

            check_error(
                    errtok,
                    "`if` returns `{}`, but `else` returns `{}`",
                    *ifbr_type,
                    *elsebr_type);
            err = true;
        }
    }

    if (err) return null;
    return ifbr_type;
}

Type* check_expr(
        CheckContext* c, 
        Expr* expr, 
        Type* cast, 
        bool cast_to_definitive_type) {
    switch (expr->kind) {
        case EXPR_KIND_INTEGER: {
            return check_integer_expr(c, expr, cast, cast_to_definitive_type);
        } break;

        case EXPR_KIND_CONSTANT: {
            return check_constant_expr(c, expr, cast);
        } break;

        case EXPR_KIND_SYMBOL: {
            return check_symbol_expr(c, expr, cast);
        } break;

        case EXPR_KIND_FUNCTION_CALL: {
            return check_function_call_expr(c, expr, cast);
        } break;

        case EXPR_KIND_BINOP: {
            return check_binop_expr(c, expr, cast);
        } break;

        case EXPR_KIND_UNOP: {
            return check_unop_expr(c, expr, cast);
        } break;

        case EXPR_KIND_CAST: {
            return check_cast_expr(c, expr, cast);
        } break;

        case EXPR_KIND_BLOCK: {
            return check_block_expr(c, expr, cast);
        } break;

        case EXPR_KIND_IF: {
            return check_if_expr(c, expr, cast);
        } break;
    }
    assert(0);
    return null;
}

void check_function_stmt(CheckContext* c, Stmt* stmt) {
    c->last_stack_offset = 0;
    if (!stmt->function.is_extern) {
        Type* body_type = check_block_expr(
                c, 
                stmt->function.body, 
                stmt->function.header->return_type);
        Type* return_type = stmt->function.header->return_type;
        stmt->function.body->type = body_type;
        if (body_type && return_type) {
            if (check_implicit_cast(c, body_type, return_type) == IMPLICIT_CAST_ERROR) {
                Token* errtok = null;
                if (stmt->function.body->block.value)
                    errtok = stmt->function.body->block.value->main_token;
                else
                    errtok = stmt->function.header->return_type->main_token;
                check_error(
                        errtok,
                        "function annotated `{}`, but returned `{}`",
                        *return_type,
                        *body_type);
            }
        }
    }
}

void check_variable_stmt(CheckContext* c, Stmt* stmt) {
    if (stmt->variable.type && stmt->variable.initializer) {
        Type* initializer_type = check_expr(
                c,
                stmt->variable.initializer,
                stmt->variable.type, 
                true);
        Type* annotated_type = stmt->variable.type;
        stmt->variable.initializer_type = initializer_type;
        if (initializer_type && annotated_type) {
            if (check_implicit_cast(c, initializer_type, annotated_type) == 
                IMPLICIT_CAST_ERROR) {
                check_error(
                        stmt->variable.identifier,
                        "cannot initialize type `{}` from `{}`",
                        *annotated_type,
                        *initializer_type);
            }
        }
    }
    else if (stmt->variable.initializer) {
        stmt->variable.type = check_expr(
                c, 
                stmt->variable.initializer,
                null,
                true);
        stmt->variable.initializer_type = stmt->variable.type;
        if (stmt->variable.type && type_is_apint(stmt->variable.type)) {
            if (check_implicit_cast(c, stmt->variable.type, builtin_type_placeholders.int32) == 
                IMPLICIT_CAST_ERROR) {
                check_error(
                        stmt->variable.identifier,
                        "cannot initialize type `{}` from `{}`",
                        *builtin_type_placeholders.int32,
                        *stmt->variable.type);
                return;
            }
        }
    }

    if (stmt->parent_func && stmt->variable.type) {
        size_t bytes = type_bytes(stmt->variable.type);
        c->last_stack_offset += bytes;
        c->last_stack_offset = 
            align_to_pow2(c->last_stack_offset, bytes);
        stmt->variable.stack_offset = c->last_stack_offset;
        stmt->parent_func->function.stack_vars_size = 
            c->last_stack_offset;
    }
}

void check_while_stmt(CheckContext* c, Stmt* stmt) {
    Type* cond_type = check_expr(c, stmt->whilelp.cond, null, true);
    if (cond_type && check_implicit_cast(
                c,
                cond_type,
                builtin_type_placeholders.boolean) == IMPLICIT_CAST_ERROR) {
        check_error(
                stmt->whilelp.cond->main_token,
                "loop condition should be `{}` but got `{}`",
                *builtin_type_placeholders.boolean,
                *cond_type);
    }
    check_block_expr(c, stmt->whilelp.body, null);
}

#define is_deref_expr(expr) \
    (expr->kind == EXPR_KIND_UNOP && \
     expr->unop.op->kind == TOKEN_KIND_STAR)

void check_assign_stmt(CheckContext* c, Stmt* stmt) {
    Type* left_type = check_expr(c, stmt->assign.left, null, true);
    Type* right_type = check_expr(c, stmt->assign.right, null, true);

    if (left_type && is_deref_expr(stmt->assign.left)) {
        Type* lhs_deref_child_type = stmt->assign.left->unop.child_type;
        assert(lhs_deref_child_type->kind == TYPE_KIND_PTR);
        if (lhs_deref_child_type->ptr.constant) {
            check_error(
                    stmt->assign.left->main_token,
                    "cannot mutate read-only location `{}`",
                    *lhs_deref_child_type);
        }
    }

    if (left_type && right_type && 
            check_implicit_cast(c, right_type, left_type) == IMPLICIT_CAST_ERROR) {
        check_error(
                stmt->main_token,
                "cannot assign to `{}` from `{}`",
                *left_type,
                *right_type);
    }
}

void check_expr_stmt(CheckContext* c, Stmt* stmt) {
    check_expr(c, stmt->expr.child, null, true);
}

void check_stmt(CheckContext* c, Stmt* stmt) {
    switch (stmt->kind) {
        case STMT_KIND_FUNCTION: {
            check_function_stmt(c, stmt);
        } break;

        case STMT_KIND_VARIABLE: {
            check_variable_stmt(c, stmt);
        } break;

        case STMT_KIND_WHILE: {
            return check_while_stmt(c, stmt);
        } break;

        case STMT_KIND_ASSIGN: {
            check_assign_stmt(c, stmt);
        } break;

        case STMT_KIND_EXPR: {
            check_expr_stmt(c, stmt);
        } break;
    }
}

void check(CheckContext* c) {
    c->error = false;
    c->last_stack_offset = 0;

    for (Stmt* stmt: c->srcfile->stmts) {
        check_stmt(c, stmt);
    }
}

