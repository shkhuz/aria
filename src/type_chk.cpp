typedef struct {
    Srcfile* srcfile;
    bool error;
} CheckContext;

typedef enum {
    IC_OK,
    IC_ERROR,
    IC_ERROR_HANDLED,
} ImplicitCastStatus;

#define check_error(token, fmt, ...) \
    error(token, fmt, ##__VA_ARGS__); \
    c->error = true

#define CHECK_IMPL_CAST(from, to) \
    ImplicitCastStatus impl_cast_status = check_implicit_cast(c, from, to)

#define IS_IMPL_CAST_STATUS(status) (impl_cast_status == status)

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
                    return IC_ERROR;
                }
                else {
                    return IC_OK;
                }
            }
            else if (builtin_type_is_apint(from->builtin.kind) || 
                     builtin_type_is_apint(to->builtin.kind)) {
                if (builtin_type_is_apint(to->builtin.kind)) {
                    // from always points to apint
                    SWAP_VARS(Type*, from, to);
                }

                if (!type_is_integer(to)) {
                    return IC_ERROR;
                }
                else if (bigint_fits(
                            &from->builtin.apint,
                            builtin_type_bytes(&to->builtin),
                            builtin_type_is_signed(to->builtin.kind))) {
                    from->builtin.kind = to->builtin.kind;
                    return IC_OK;
                }
                else {
                    check_error(
                            from->builtin.token, 
                            "literal value outside `{}` range",
                            *to);
                    addinfo("`{:n}` can only store values from {} to {}",
                            *to, 
                            builtin_type_get_min_val(to->builtin.kind),
                            builtin_type_get_max_val(to->builtin.kind));
                    return IC_ERROR_HANDLED;
                }
            }
            else if (from->builtin.kind == to->builtin.kind) {
                if (from->builtin.kind == BUILTIN_TYPE_KIND_BOOLEAN ||
                    from->builtin.kind == BUILTIN_TYPE_KIND_VOID) {
                    return IC_OK;
                } 
                else {
                    return IC_ERROR;
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
                        // TODO: remove this func call
                        type_get_child(from),
                        type_get_child(to));
            }
        }
        else if (from->kind == TYPE_KIND_ARRAY) {
            if (from->array.lennum == to->array.lennum) {
                return type_is_equal(
                        from->array.elem_type,
                        to->array.elem_type,
                        true) ? IC_OK : IC_ERROR;
            }
        }
        else if (from->kind == TYPE_KIND_CUSTOM) {
            if (from->custom.kind == to->custom.kind) {
                switch (from->custom.kind) {
                    case CUSTOM_TYPE_KIND_STRUCT: {
                        if (from->custom.ref == to->custom.ref) {
                            return IC_OK;
                        }
                    } break;

                    case CUSTOM_TYPE_KIND_SLICE: {
                        // Refer diagram above for an explaination
                        if (!from->custom.slice.constant || to->custom.slice.constant) {
                            return type_is_equal(
                                    from->custom.slice.child,
                                    to->custom.slice.child,
                                    true) ? IC_OK : IC_ERROR;
                        }
                    } break;
                }
            }
        }
    }
    else if ((from->kind == TYPE_KIND_PTR && from->ptr.child->kind == TYPE_KIND_ARRAY) &&
             type_is_slice(to)) {
        // cast from pointer-to-array to a slice 
        // `*(const?) [#]type` --> `[](const?) type
        
        // Refer diagram above for an explaination
        if (!from->ptr.constant || to->custom.slice.constant) { 
            return type_is_equal(
                    from->ptr.child->array.elem_type,
                    to->custom.slice.child,
                    true) ? IC_OK : IC_ERROR;
        }
    }
    return IC_ERROR;
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
        // left always points to apint
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

    CHECK_IMPL_CAST(apint_type, int_type);
    if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
        check_error(
                apint_type->main_token,
                "expression cannot be converted to `{}`",
                *int_type);
        return false;
    }
    else if (IS_IMPL_CAST_STATUS(IC_ERROR_HANDLED)) {
        return false;
    }
    /* apint_type->builtin.kind = int_type->builtin.kind; */
    expr->type = int_type;
    return true;
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
                "`{}` cannot be converted to `{}`",
                *apint,
                *builtin_type_placeholders.int32);
    }
}

Type* check_integer_expr(
        CheckContext* c, 
        Expr* expr, 
        Type* cast, 
        bool cast_to_definitive_type);

Type* check_type(CheckContext* c, Type* type) {
    bool err = false;
    switch (type->kind) {
        case TYPE_KIND_BUILTIN: {
            return type;
        } break;

        case TYPE_KIND_PTR: {
            if (!check_type(c, type->ptr.child)) return null;
            return type;
        } break;

        case TYPE_KIND_ARRAY: {
            if (check_integer_expr(
                    c, 
                    type->array.len, 
                    builtin_type_placeholders.uint64, 
                    true) != null) {
                type->array.lennum = bigint_get_lsd(type->array.len->integer.val);
            }
            else err = true;

            CHECK_IMPL_CAST(type->array.elem_type, builtin_type_placeholders.void_kind);
            if (!IS_IMPL_CAST_STATUS(IC_ERROR) && !IS_IMPL_CAST_STATUS(IC_ERROR_HANDLED)) {
                check_error(
                        type->array.elem_type->main_token,
                        "array of `{}` types is invalid",
                        *type->array.elem_type);
                err = true;
            }
            check_type(c, type->array.elem_type);

            if (err) return null;
            else return type;
        } break;

        case TYPE_KIND_CUSTOM: {
            switch (type->custom.kind) {
                case CUSTOM_TYPE_KIND_STRUCT: {
                    return type;
                } break;

                case CUSTOM_TYPE_KIND_SLICE: {
                    if (!check_type(c, type->custom.slice.child)) return null;
                    return type;
                } break;
            }
        } break;
    }
    return null;
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
    Type* type = builtin_type_new(
            expr->main_token,
            BUILTIN_TYPE_KIND_APINT);
    type->builtin.apint = *expr->integer.val;
    
    if (!cast && !cast_to_definitive_type) {
        expr->type = type;
        return type;
    }
    else {
        if (!cast) cast = builtin_type_placeholders.int32;
        CHECK_IMPL_CAST(cast, type);
        if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
            check_error(
                    expr->integer.integer,
                    "`{}` cannot be converted to `{}`",
                    *type,
                    *cast);
            return null;
        }
        else if (IS_IMPL_CAST_STATUS(IC_ERROR_HANDLED)) {
            return null;
        }
    }
    expr->type = cast;
    return cast;
}

Type* check_constant_expr(CheckContext* c, Expr* expr, Type* cast) {
    Type* type = null;
    switch (expr->constantexpr.kind) {
        case CONSTANT_KIND_BOOLEAN_TRUE:
        case CONSTANT_KIND_BOOLEAN_FALSE: {
            type = builtin_type_placeholders.boolean;
        } break;

        case CONSTANT_KIND_NULL: {
            type = builtin_type_placeholders.void_ptr;
        } break;

        case CONSTANT_KIND_STRING: {
            type = ptr_type_placeholder_new(
                    true,
                    array_type_placeholder_new(
                        expr->constantexpr.token->lexeme.size(),
                        builtin_type_placeholders.uint8));
        } break;
    }
    expr->type = type;
    return type;
}

Type* check_symbol_expr(CheckContext* c, Expr* expr, Type* cast) {
    assert(expr->symbol.ref);
    if (expr->symbol.ref->kind != STMT_KIND_FUNCTION) {
        Type* type = stmt_get_type(expr->symbol.ref);
        expr->type = type;
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
    std::vector<Expr*>& args = expr->function_call.args;
    size_t arg_len = args.size();
    assert(expr->function_call.callee->symbol.ref->kind == STMT_KIND_FUNCTION);

    std::vector<Stmt*>& params = expr->function_call.callee->symbol.ref->function.header->params;
    size_t param_len = params.size();
    Type* callee_return_type = expr->function_call.callee->symbol.ref->function.header->return_type;

    if (arg_len != param_len) {
        if (arg_len < param_len) {
            std::string fmt = fmt::format(
                    "expected {} additional argument(s) of type ", 
                    param_len - arg_len);
            for (size_t i = arg_len; i < param_len; i++) {
                fmt = fmt + fmt::format("`{}`", *params[i]->param.type);
                if (i+1 < param_len) {
                    fmt = fmt + ", ";
                }
            }
            
            check_error(
                    expr->function_call.rparen,
                    fmt);
            note(
                    params[arg_len]->param.identifier,
                    "parameter(s) declared here");
        }
        else {
            check_error(
                    args[param_len]->main_token,
                    "extra argument(s) supplied");
        }
        return callee_return_type;
    }

    for (size_t i = 0; i < args.size(); i++) {
        Type* param_type = params[i]->param.type;
        Type* arg_type = check_expr(c, args[i], param_type, true);
        /* args[i]->type = arg_type; */
        if (arg_type && param_type) {
            CHECK_IMPL_CAST(arg_type, param_type);
            if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
                check_error(
                        args[i]->main_token,
                        "argument type `{}` cannot be converted to `{}`",
                        *arg_type,
                        *param_type);
                note(
                        param_type->main_token,
                        "parameter type annotated here");
            }
        }
    }
    expr->type = callee_return_type;
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
                bool return_left = true;
                ImplicitCastStatus impl_cast_status;
                if (type_bytes(right_type) > type_bytes(left_type)) {
                    impl_cast_status = check_implicit_cast(c, left_type, right_type);
                    return_left = false;
                }
                else {
                    impl_cast_status = check_implicit_cast(c, right_type, left_type);
                }
                if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
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
                    expr->binop.folded = true;
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
            //TODO: here
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
                if (token_is_magnitude_cmp_op(expr->binop.op)) {
                    bool is_left_int = type_is_integer(left_type);
                    bool is_right_int = type_is_integer(right_type);
                    if (!is_left_int && !is_right_int) {
                        check_error(
                                expr->binop.op,
                                "`{}` requires integer operands, but given `{}` and `{}`",
                                *expr->binop.op,
                                *left_type, 
                                *right_type);
                        return null;
                    }
                    else if (!is_left_int || !is_right_int) {
                        std::string erroperand;
                        Type* errtype;
                        if (!is_left_int) {
                            erroperand = "left";
                            errtype = left_type;
                        }
                        else {
                            erroperand = "right";
                            errtype = right_type;
                        }
                        check_error(
                                expr->binop.op,
                                "`{}` requires integer operands, but {} operand is `{}`",
                                *expr->binop.op,
                                erroperand, 
                                *errtype);
                        return null;
                    }
                }
                ImplicitCastStatus impl_cast_status;
                if (type_bytes(right_type) > type_bytes(left_type)) {
                    impl_cast_status = check_implicit_cast(c, left_type, right_type);
                }
                else {
                    impl_cast_status = check_implicit_cast(c, right_type, left_type);
                }
                if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
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
                if (cast) {
                    CHECK_IMPL_CAST(new_apint, cast);
                    if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
                        // TODO: i forgot: why assert(0) here?
                        assert(0);
                        return null;
                    }
                    else if (IS_IMPL_CAST_STATUS(IC_ERROR_HANDLED)) {
                        return null;
                    }
                }
                expr->unop.folded = true;
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
            CHECK_IMPL_CAST(child_type, builtin_type_placeholders.boolean);
            if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
                check_error(
                        expr->main_token,
                        "`!` operator requires a boolean operand",
                        *child_type);
            }
            else {
                return child_type;
            }
        }
        else if (expr->unop.op->kind == TOKEN_KIND_AMP) {
            CHECK_IMPL_CAST(child_type, builtin_type_placeholders.void_kind);
            if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
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
                Type* ty = ptr_type_placeholder_new(constant, child_type);
                expr->type = ty;
                return ty;
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
    to_type = check_type(c, to_type);
    expr->type = to_type;

    if (left_type) {
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
        else if (left_type->kind == TYPE_KIND_PTR && type_is_integer(to_type)) {
            return to_type;
        }
        /* else if ((left_type->kind == TYPE_KIND_PTR && left_type->ptr.child->kind == TYPE_KIND_ARRAY) && */
        /*          type_is_slice(to_type)) { */
        /*     // cast from pointer-to-array to a slice */ 
        /*     // `*(const?) [#]type` --> `[](const?) type */
            
        /*     // Refer diagram above for an explaination */
        /*     if (!left_type->ptr.constant || to_type->custom.slice.constant) { */ 
        /*         if (type_is_equal( */
        /*                 left_type->ptr.child->array.elem_type, */
        /*                 to_type->custom.slice.child, */
        /*                 true)) { */
        /*             return to_type; */
        /*         } */
        /*         else { */
        /*             check_error( */
        /*                     expr->main_token, */
        /*                     "cannot cast from `{}` to `{}`", */
        /*                     *left_type, */
        /*                     *to_type); */
        /*         } */
        /*     } */
        /* } */
        else if (left_type->kind == TYPE_KIND_PTR && to_type->kind == TYPE_KIND_PTR) {
            if (left_type->ptr.constant && !to_type->ptr.constant) {
                check_error(
                        expr->main_token,
                        "cast to `{}` discards const-ness of `{}`",
                        *to_type,
                        *left_type);
                addinfo("converting a const-pointer to a mutable pointer is invalid");
                addinfo("consider adding `const`: `*const {:n}`",
                        *to_type->ptr.child);
            }
            else {
                return to_type;
            }
        }
        else if (type_is_slice(left_type) && type_is_slice(to_type)) {
            CHECK_IMPL_CAST(left_type, to_type);
            if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
                check_error(
                        expr->main_token,
                        "cannot cast from `{}` to `{}`",
                        *left_type,
                        *to_type);
            }
            else if (IS_IMPL_CAST_STATUS(IC_ERROR)) {}  
            else {
                return to_type;
            }
        }
        else {
            CHECK_IMPL_CAST(left_type, to_type);
            if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
                check_error(
                        expr->main_token,
                        "cannot cast from `{}` to `{}`",
                        *left_type,
                        *to_type);
            }
            else if (IS_IMPL_CAST_STATUS(IC_OK)) {
                return to_type;
            }
        }
    }
    return null;
}

Type* check_block_expr(CheckContext* c, Expr* expr, Type* cast) {
    for (Stmt* stmt: expr->block.stmts) {
        check_stmt(c, stmt);
    }
    if (expr->block.value) {
        expr->type = check_expr(c, expr->block.value, cast, true);
    }
    else {
        expr->type = builtin_type_placeholders.void_kind;
    }
    return expr->type;
}

Type* check_if_branch(CheckContext* c, IfBranch* br, Type* cast) {
    if (br->cond) {
        Type* cond_type = check_expr(c, br->cond, null, true);
        if (cond_type) {
            CHECK_IMPL_CAST(cond_type, builtin_type_placeholders.boolean);
            if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
                check_error(
                        br->cond->main_token,
                        "branch condition should be `{}` but got `{}`",
                        *builtin_type_placeholders.boolean,
                        *cond_type);
            }
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
    expr->type = ifbr_type;
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
        CHECK_IMPL_CAST(elseifbr_types[i], ifbr_type);
        if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
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
        else if (IS_IMPL_CAST_STATUS(IC_ERROR_HANDLED)) {
            err = true;
        }
    }

    if (expr->iff.elsebr) {
        CHECK_IMPL_CAST(elsebr_type, ifbr_type);
        if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
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
        else if (IS_IMPL_CAST_STATUS(IC_ERROR_HANDLED)) {
            err = true;    
        } 
            
    }

    if (err) return null;
    return ifbr_type;
}

Type* check_while_expr(CheckContext* c, Expr* expr, Type* cast) {
    Type* cond_type = check_expr(c, expr->whilelp.cond, null, true);
    if (cond_type) {
        CHECK_IMPL_CAST(cond_type, builtin_type_placeholders.boolean);
        if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
            check_error(
                    expr->whilelp.cond->main_token,
                    "loop condition should be `{}` but got `{}`",
                    *builtin_type_placeholders.boolean,
                    *cond_type);
        }
    }

    Type* val = check_block_expr(c, expr->whilelp.body, cast);
    if (val) {
        CHECK_IMPL_CAST(val, builtin_type_placeholders.void_kind);
        if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
            check_error(
                    expr->whilelp.body->block.value->main_token,
                    "`while` cannot implicitly return a value");
            return null;
        }
        else if (IS_IMPL_CAST_STATUS(IC_ERROR_HANDLED)) return null;
    }
    return val;
}

#define field_access_unknown_field() \
    check_error( \
            expr->fieldacc.right, \
            "`{}` does not contain a field `{}`", \
            *left_type, \
            *expr->fieldacc.right)

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

        case EXPR_KIND_ARRAY_LITERAL: {
            Type* target_elem_type = null;
            if (cast && cast->kind == TYPE_KIND_ARRAY) {
                target_elem_type = cast->array.elem_type;
            }

            // `first_elem_type` defaults to target or i32 in case the array literal is 
            // empty.
            Type* first_elem_type = target_elem_type ? target_elem_type : builtin_type_placeholders.int32;
            bool err = false;
            for (size_t i = 0; i < expr->arraylit.elems.size(); i++) {
                Type* ty = check_expr(c, expr->arraylit.elems[i], target_elem_type, true);
                if (i == 0) {
                    if (!ty) {
                        err = true;
                        break;
                    }
                    first_elem_type = ty;
                }
                if (i != 0 && ty && first_elem_type) {
                    CHECK_IMPL_CAST(ty, first_elem_type);
                    if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
                        check_error(
                                expr->arraylit.elems[i]->main_token,
                                "expected element type `{}` but got `{}`",
                                *first_elem_type,
                                *ty);
                        continue;
                    }
                    else if (IS_IMPL_CAST_STATUS(IC_ERROR_HANDLED)) {
                        continue;
                    }
                }
            }
            
            if (err) return null;
            

            Type* literal_type = array_type_new(null, first_elem_type, null);
            literal_type->array.lennum = expr->arraylit.elems.size();
            expr->type = literal_type;
            return literal_type;
        } break;

        case EXPR_KIND_SYMBOL: {
            return check_symbol_expr(c, expr, cast);
        } break;

        case EXPR_KIND_FUNCTION_CALL: {
            return check_function_call_expr(c, expr, cast);
        } break;

        case EXPR_KIND_FIELD_ACCESS: {
            Type* left_type = check_expr(c, expr->fieldacc.left, null, true);
            expr->fieldacc.left_type = left_type;
            expr->constant = expr->fieldacc.left->constant;
            if (left_type) {
                if (left_type->kind == TYPE_KIND_CUSTOM) {
                    Stmt* field = null;
                    for (Stmt* f: left_type->custom.ref->structure.fields) {
                        if (is_token_lexeme_eq(f->field.identifier, expr->fieldacc.right)) {
                            field = f;
                            break;
                        }
                    }
                    
                    if (field) {
                        expr->fieldacc.rightref = field;
                        expr->type = field->field.type;
                        return field->field.type;
                    }
                    else {
                        field_access_unknown_field();
                        return null;
                    }
                }
                else if (left_type->kind == TYPE_KIND_ARRAY) {
                    if (expr->fieldacc.right->lexeme == "len") {
                        expr->constant = true;
                        expr->type = builtin_type_placeholders.uint64;
                        return expr->type;
                    }
                    else {
                        field_access_unknown_field();
                        return null;
                    }
                }
                else {
                    check_error(
                            expr->main_token,
                            "cannot access member fields from type `{}`",
                            *left_type);
                    return null;
                }
            }
            return null;
        } break;

        case EXPR_KIND_INDEX: {
            Type* left_type = check_expr(c, expr->index.left, null, true);
            expr->index.left_type = left_type;
            Type* result = null;
            if (left_type) {
                if (left_type->kind == TYPE_KIND_ARRAY) {
                    result = left_type->array.elem_type;
                }
                else if (left_type->kind == TYPE_KIND_PTR) {
                    result = left_type->ptr.child;
                }
                else if (type_is_slice(left_type)) {
                    result = left_type->custom.slice.child;
                }
                else {
                    check_error(
                            expr->index.lbrack,
                            "left operand is not an array, slice or a pointer");
                    addinfo("left operand type is `{:n}`, but index operator `[]`", *left_type);
                    addinfo("requires an array, slice or a pointer operand");
                }
            }

            Type* idx_type = check_expr(c, expr->index.idx, builtin_type_placeholders.uint64, true);
            if (idx_type) {
                CHECK_IMPL_CAST(idx_type, builtin_type_placeholders.uint64);
                if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
                    check_error(
                            expr->index.idx->main_token,
                            "expected `{}` for index, but got `{}`",
                            *builtin_type_placeholders.uint64,
                            *idx_type);
                }
            }

            expr->type = result;
            return result;
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
        
        case EXPR_KIND_WHILE: {
            return check_while_expr(c, expr, cast);
        } break;
    }
    assert(0);
    return null;
}

void check_function_stmt(CheckContext* c, Stmt* stmt) {
    if (!stmt->function.header->is_extern) {
        Type* body_type = check_block_expr(
                c, 
                stmt->function.body, 
                stmt->function.header->return_type);
        Type* return_type = stmt->function.header->return_type;
        stmt->function.body->type = body_type;
        if (body_type && return_type) {
            CHECK_IMPL_CAST(body_type, return_type);
            if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
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
    if (stmt->variable.type && !stmt->variable.global) {
        stmt->variable.type = check_type(c, stmt->variable.type);
    }

    if (stmt->variable.type && stmt->variable.initializer) {
        Type* initializer_type = check_expr(
                c,
                stmt->variable.initializer,
                stmt->variable.type, 
                true);
        Type* annotated_type = stmt->variable.type;
        stmt->variable.initializer_type = initializer_type;
        if (initializer_type && annotated_type) {
            CHECK_IMPL_CAST(initializer_type, annotated_type);
            if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
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
            CHECK_IMPL_CAST(stmt->variable.type, builtin_type_placeholders.int32);
            if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
                check_error(
                        stmt->variable.identifier,
                        "cannot initialize type `{}` from `{}`",
                        *builtin_type_placeholders.int32,
                        *stmt->variable.type);
                return;
            }
            else if (IS_IMPL_CAST_STATUS(IC_ERROR_HANDLED)) {
                return;
            }
        }
    }
    
    if (stmt->variable.type) {
        if (stmt->variable.type->kind == TYPE_KIND_ARRAY) {
            stmt->variable.type->array.constant = stmt->variable.constant;
        }
        else {
            CHECK_IMPL_CAST(stmt->variable.type, builtin_type_placeholders.void_kind);
            if (!IS_IMPL_CAST_STATUS(IC_ERROR) && !IS_IMPL_CAST_STATUS(IC_ERROR_HANDLED)) {
                check_error(
                        stmt->variable.identifier,
                        "useless variable due to type `{}`",
                        *builtin_type_placeholders.void_kind);
                return;
            }
        }
    }
}

#define is_deref_expr(expr) \
    (expr->kind == EXPR_KIND_UNOP && \
     expr->unop.op->kind == TOKEN_KIND_STAR)

void check_assign_stmt(CheckContext* c, Stmt* stmt) {
    Type* left_type = check_expr(c, stmt->assign.left, null, true);
    stmt->assign.left_type = left_type;
    Type* right_type = check_expr(c, stmt->assign.right, left_type, true);
    stmt->assign.right_type = right_type;

    if (left_type && is_deref_expr(stmt->assign.left)) {
        Type* lhs_deref_child_type = stmt->assign.left->unop.child_type;
        bool constant;
        if (lhs_deref_child_type->kind == TYPE_KIND_PTR) {
            constant = lhs_deref_child_type->ptr.constant;
        }
        else if (type_is_slice(lhs_deref_child_type)) {
            constant = lhs_deref_child_type->custom.slice.constant;
        }
        if (constant) {
            check_error(
                    stmt->assign.left->main_token,
                    "cannot mutate read-only location with type `{}`",
                    *lhs_deref_child_type);
        }
    }
    else if (left_type && stmt->assign.left->kind == EXPR_KIND_INDEX) {
        Type* lhs_operand_type = stmt->assign.left->index.left_type;
        bool constant;
        if (lhs_operand_type->kind == TYPE_KIND_PTR) {
            constant = lhs_operand_type->ptr.constant;
        }
        else if (lhs_operand_type->kind == TYPE_KIND_ARRAY) {
            constant = lhs_operand_type->array.constant;
        }
        else if (type_is_slice(lhs_operand_type)) {
            constant = lhs_operand_type->custom.slice.constant;
        }

        if (constant) {
            check_error(
                    stmt->assign.left->index.lbrack,
                    "cannot mutate read-only location with type `{}`",
                    *lhs_operand_type);
        }
    }
    else if (left_type && stmt->assign.left->kind == EXPR_KIND_FIELD_ACCESS &&
             stmt->assign.left->constant) {
        // This cannot be checked in the resolve pass because we do not have
        // the struct type info prior to type_chk pass.
        check_error(
                stmt->main_token,
                "cannot modify immutable constant");
    }

    if (left_type && right_type) {
        CHECK_IMPL_CAST(right_type, left_type);
        if (IS_IMPL_CAST_STATUS(IC_ERROR)) {
            check_error(
                    stmt->main_token,
                    "cannot assign to `{}` from `{}`",
                    *left_type,
                    *right_type);
        }
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

        case STMT_KIND_PARAM: {
            stmt->param.type = check_type(c, stmt->param.type);
            CHECK_IMPL_CAST(stmt->param.type, builtin_type_placeholders.void_kind);
            if (!IS_IMPL_CAST_STATUS(IC_ERROR) && !IS_IMPL_CAST_STATUS(IC_ERROR_HANDLED)) {
                check_error(
                        stmt->param.identifier,
                        "useless parameter due to type `{}`",
                        *builtin_type_placeholders.void_kind);
                return;
            }
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

    for (Stmt* stmt: c->srcfile->stmts) {
        switch (stmt->kind) {
            case STMT_KIND_STRUCT: {
                for (Stmt* field: stmt->structure.fields) {
                    field->field.type = check_type(c, field->field.type);
                }
            } break;

            case STMT_KIND_FUNCTION: {
                for (Stmt* param: stmt->function.header->params) {
                    check_stmt(c, param);
                }
                stmt->function.header->return_type = check_type(
                        c, 
                        stmt->function.header->return_type);
            } break;

            case STMT_KIND_VARIABLE: {
                if (stmt->variable.type) {
                    stmt->variable.type = check_type(c, stmt->variable.type);
                }
            } break;
        }
    }
    for (Stmt* stmt: c->srcfile->stmts) {
        check_stmt(c, stmt);
    }
}
