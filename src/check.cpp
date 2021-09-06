struct Checker {
    Srcfile* srcfile;
    bool error;

    Checker(Srcfile* srcfile) 
        : srcfile(srcfile) {
        this->error = false;
    }

    Type** symbol(Expr* expr) {
        assert(expr->symbol.ref);
        return Stmt::get_type(expr->symbol.ref);
    }

    Type** scoped_block(Expr* expr) {
        for (auto& stmt: expr->scoped_block.stmts) {
            this->stmt(stmt);
        }
        return nullptr;
    }

    Type** binop(Expr* expr) {
        Type** left_type = this->expr(expr->binop.left);
        Type** right_type = this->expr(expr->binop.right);
    }

    Type** expr(Expr* expr) {
        switch (expr->kind) {
            case ExprKind::symbol: {
                return this->symbol(expr);
            } break;

            case ExprKind::scoped_block: {
                this->scoped_block(expr);
            } break;

            case ExprKind::binop: {
                this->binop(expr);
            } break;

            default: {
                assert(0);
            } break;
        }
        return nullptr;
    }

    bool implicit_cast(Type** from, Type** to) {
        assert(from && to);
        if ((*from)->kind == (*to)->kind) {
            if ((*from)->kind == TypeKind::builtin) {
                if (builtin_type::is_integer((*from)->builtin.kind) &&
                    builtin_type::is_integer((*to)->builtin.kind)) {
                    if (builtin_type::bytes(&(*from)->builtin) >
                        builtin_type::bytes(&(*to)->builtin) ||
                        builtin_type::is_signed((*from)->builtin.kind) !=
                        builtin_type::is_signed((*to)->builtin.kind)) {
                        return false;
                    } else {
                        return true;
                    }
                } else if ((*from)->builtin.kind == (*to)->builtin.kind) {
                    if ((*from)->builtin.kind == BuiltinTypeKind::boolean ||
                        (*from)->builtin.kind == BuiltinTypeKind::void_kind) {
                        return true;
                    } else {
                        return false;
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
        return false;
    }

    void function(Stmt* stmt) {
        this->expr(stmt->function.body);
    }

    void variable(Stmt* stmt) {
        if (stmt->variable.type && stmt->variable.initializer) {
            Type** initializer_type = this->expr(stmt->variable.initializer);
            Type** annotated_type = stmt->variable.type;
            if (initializer_type && annotated_type && 
                    !this->implicit_cast(initializer_type, annotated_type)) {
                error(
                        stmt->variable.identifier,
                        "cannot initialize from `", **initializer_type,
                        "` to `", **annotated_type, "`");
            }
        };
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
    }
};
