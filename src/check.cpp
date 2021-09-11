enum class ImplicitCastStatus {
    ok, 
    error_handled, 
    error,
};

struct Checker {
    Srcfile* srcfile;
    std::vector<Stmt*> not_inferred_variables;
    bool error;

    Checker(Srcfile* srcfile) 
        : srcfile(srcfile) {
        this->error = false;
    }

    Type** number(Expr* expr, Type** cast) {
        Type** type = builtin_type_new(expr->number.number, BuiltinTypeKind::not_inferred);
        (*type)->builtin.val = expr->number.val;
        if (cast && (*cast)->is_integer()) {
            if (this->implicit_cast(type, cast) == ImplicitCastStatus::error) {
                return type;
            } else {
                return cast;
            }
        }
        return type;
    }

    Type** symbol(Expr* expr, Type** cast) {
        assert(expr->symbol.ref);
        Type** ty = Stmt::get_type(expr->symbol.ref);
        if (cast) {
            if (this->implicit_cast(ty, cast) == ImplicitCastStatus::error) {
                return ty;
            } else {
                *ty = *cast;
                return cast;
            }
        }
        return ty;
    }

    Type** scoped_block(Expr* expr) {
        for (auto& stmt: expr->scoped_block.stmts) {
            this->stmt(stmt);
        }
        return nullptr;
    }

    Type** binop(Expr* expr, Type** cast) {
        Type** left_type = this->expr(expr->binop.left, cast);
        Type** right_type = this->expr(expr->binop.right, cast);
        if ((*left_type)->is_not_inferred() && 
                (*right_type)->is_not_inferred()) {
        } else if (left_type && right_type && 
                this->implicit_cast(right_type, left_type) == ImplicitCastStatus::error) {
            error(
                    expr->binop.op, 
                    "type mismatch: `", 
                    **left_type, 
                    "` and `", 
                    **right_type, 
                    "`");

        }
        return left_type;
    }

    Type** expr(Expr* expr, Type** cast) {
        switch (expr->kind) {
            case ExprKind::number: {
                return this->number(expr, cast);
            } break;

            case ExprKind::symbol: {
                return this->symbol(expr, cast);
            } break;

            case ExprKind::scoped_block: {
                return this->scoped_block(expr);
            } break;

            case ExprKind::binop: {
                return this->binop(expr, cast);
            } break;

            default: {
                assert(0);
            } break;
        }
        assert(0);
        return nullptr;
    }

    ImplicitCastStatus implicit_cast(Type** from, Type** to) {
        assert(from && to);
        if ((*from)->kind == (*to)->kind) {
            if ((*from)->kind == TypeKind::builtin) {
                if (builtin_type::is_integer((*from)->builtin.kind) &&
                    builtin_type::is_integer((*to)->builtin.kind)) {
                    if (builtin_type::bytes(&(*from)->builtin) >
                        builtin_type::bytes(&(*to)->builtin) ||
                        builtin_type::is_signed((*from)->builtin.kind) !=
                        builtin_type::is_signed((*to)->builtin.kind)) {
                        return ImplicitCastStatus::error;
                    } else {
                        return ImplicitCastStatus::ok;
                    }
                } else if ((*from)->builtin.kind == BuiltinTypeKind::not_inferred || 
                           (*to)->builtin.kind == BuiltinTypeKind::not_inferred) {
                    if ((*to)->builtin.kind == BuiltinTypeKind::not_inferred) {
                        swap_vars(Type**, from, to);
                    }
                    if (!bigint_fits((*from)->builtin.val, builtin_type::bytes(&(*to)->builtin), builtin_type::is_signed((*to)->builtin.kind) ? BIGINT_SIGN_NEG : BIGINT_SIGN_ZPOS)) {
                        error(
                                (*from)->builtin.token, 
                                "integer cannot be converted to `",
                                **to, "`");
                        return ImplicitCastStatus::error_handled;
                    } else {
                        return ImplicitCastStatus::ok;
                    }
                } else if ((*from)->builtin.kind == (*to)->builtin.kind) {
                    if ((*from)->builtin.kind == BuiltinTypeKind::boolean ||
                        (*from)->builtin.kind == BuiltinTypeKind::void_kind) {
                        return ImplicitCastStatus::ok;
                    } else {
                        return ImplicitCastStatus::error;
                    }
                }
            } else if ((*from)->kind == TypeKind::ptr) {
                // There are four cases:
                //           to           C = const
                //      |---------|       M = mutable
                //      |   | C M |       O = okay to cast
                //      |---------|       ! = cannot cast
                // from | C | O ! |
                //      | M | O O |
                //      |---------|
                // 
                // These four cases are handled by this one line:
                if (!(*from)->ptr.constant || (*to)->ptr.constant) {
                    return this->implicit_cast(
                            (*from)->get_child(),
                            (*to)->get_child());
                }
            }
        }
        return ImplicitCastStatus::error;
    }

    void function(Stmt* stmt) {
        this->expr(stmt->function.body, stmt->function.header->ret_type);
    }

    void variable(Stmt* stmt) {
        if (stmt->variable.type && stmt->variable.initializer) {
            Type** initializer_type = this->expr(
                    stmt->variable.initializer,
                    stmt->variable.type);
            Type** annotated_type = stmt->variable.type;
            if (initializer_type && annotated_type && 
                    this->implicit_cast(initializer_type, annotated_type) == ImplicitCastStatus::error) {
                error(
                        stmt->variable.identifier,
                        "cannot initialize from `", 
                        **initializer_type,
                        "` to `", 
                        **annotated_type, 
                        "`");
            }
        } else if (stmt->variable.initializer) {
            stmt->variable.type = this->expr(
                    stmt->variable.initializer, 
                    nullptr);
        }
        std::cout << 
            *stmt->variable.identifier << 
            ": " << 
            **stmt->variable.type << 
            std::endl;

        if ((*stmt->variable.type)->kind == TypeKind::builtin &&
            (*stmt->variable.type)->builtin.kind == BuiltinTypeKind::not_inferred) {
            not_inferred_variables.push_back(stmt);
        } else {
            if (stmt->variable.function) {
                stmt->variable.function->function.stack_vars_size += 
                    (*stmt->variable.type)->bytes();
            }
        }
    }

    void stmt(Stmt* stmt) {
        switch (stmt->kind) {
            case StmtKind::function: {
                this->function(stmt);
            } break;

            case StmtKind::variable: {
                this->variable(stmt);
            } break;
        }
    }

    void run() {
        for (auto& stmt: this->srcfile->stmts) {
            this->stmt(stmt);
        }
        
        for (auto& var: this->not_inferred_variables) {
            if ((*var->variable.type)->builtin.kind == BuiltinTypeKind::not_inferred) {
                if (implicit_cast(var->variable.type, builtin_type_placeholders.i32) == ImplicitCastStatus::error) {
                    assert(0);
                }
                *(var->variable.type) = *(builtin_type_placeholders.i32);
            }
            if (var->variable.function) {
                var->variable.function->function.stack_vars_size += 
                    (*var->variable.type)->bytes();
            }
        }
    }
};
