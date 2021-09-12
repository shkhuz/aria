struct Scope {
    Scope* parent;
    std::vector<Stmt*> stmts;

    Scope(Scope* parent)
        : parent(parent) {
    }
};

#define scope_new(name) \
    Scope* name = new Scope(this->current_scope); \
    this->current_scope = name;

#define scope_init(name) \
    name = new Scope(this->current_scope); \
    this->current_scope = name;

#define scope_revert(name) \
    this->current_scope = name->parent;

enum class ScopeStatusKind {
    local,
    parent,
    unresolved,
};

struct ScopeStatus {
    ScopeStatusKind kind;
    Stmt* stmt;
};

struct Resolver {
    Srcfile* srcfile;
    Scope* global_scope;
    Scope* current_scope;
    Stmt* current_function;
    bool error;

    Resolver(Srcfile* srcfile)
        : srcfile(srcfile) {
        this->global_scope = new Scope(nullptr);
        this->current_scope = this->global_scope;
        this->current_function = nullptr;
        this->error = false;
    }

    Stmt* search_in_scope(Token* identifier, std::vector<Stmt*>& scope) {
        for (auto& stmt: scope) {
            if (Token::is_lexeme_eq(identifier, stmt->main_token)) {
                return stmt;
            }
        }
        return nullptr;
    }

    ScopeStatus search_in_current_scope_rec(Token* identifier) {
        Scope* scope = this->current_scope;
        while (scope != nullptr) {
            for (auto& stmt: scope->stmts) {
                if (Token::is_lexeme_eq(identifier, stmt->main_token)) {
                    return ScopeStatus {
                        scope == this->current_scope ?
                            ScopeStatusKind::local :
                            ScopeStatusKind::parent,
                        stmt,
                    };
                }
            }
            scope = scope->parent;
        }
        return ScopeStatus {
            ScopeStatusKind::unresolved,
            nullptr,
        };
    }

    void cpush_in_scope(Stmt* stmt) {
        Token* identifier = stmt->main_token;
        BuiltinTypeKind kind = builtin_type::str_to_kind(identifier->lexeme);
        if (kind != BuiltinTypeKind::none) {
            error(
                    identifier,
                    "cannot use type as a symbol");
            return;
        }

        bool search_error = false;
        ScopeStatus status = search_in_current_scope_rec(identifier);
        if (status.kind == ScopeStatusKind::local) {
            error(
                    identifier,
                    "redeclaration of symbol `",
                    identifier->lexeme,
                    "`");
            search_error = true;
        } else if (status.kind == ScopeStatusKind::parent) {
            msg::warning(
                    identifier,
                    "`",
                    identifier->lexeme,
                    "` shadows symbol");
        }

        if (status.kind == ScopeStatusKind::local ||
            status.kind == ScopeStatusKind::parent) {
            msg::note(
                    status.stmt->main_token,
                    "...previously declared here");
        }

        if (search_error) return;
        this->current_scope->stmts.push_back(stmt);
    }

    // This function checks if the stmt 
    // isn't already declared, rather than 
    // checking the stmt itself.
    // The `ignore_function_level_stmt`
    // parameter tells the function whether to check
    // variables are already defined or not. 
    // If the value is `true`, the check is ignored.
    // This parameter is useful for when checking top-
    // level declarations, where we want to check for 
    // variable redeclarations in one-pass, to have 
    // order-independant declarations.
    // The `false` value is useful for when we are 
    // checking variables inside a function, where 
    // variables are defined top-to-botton and not 
    // order-independantly.
    void pre_decl_stmt(Stmt* stmt, bool ignore_function_level_stmt) {
        switch (stmt->kind) {
            case StmtKind::function: {
                this->cpush_in_scope(stmt);
            } break;

            case StmtKind::variable: {
                if (!ignore_function_level_stmt) {
                    this->cpush_in_scope(stmt);
                }
            } break;
        }
    }

    void ptr_type(Type** type) {
        this->type((*type)->ptr.child);
    }

    void type(Type** type) {
        switch ((*type)->kind) {
            case TypeKind::builtin: {
            }  break;

            case TypeKind::ptr: {
                this->ptr_type(type);
            } break;
        }
    }

    Stmt* assert_in_current_scope_rec(Expr* expr) {
        assert(expr->kind == ExprKind::symbol);
        ScopeStatus status = 
            this->search_in_current_scope_rec(expr->main_token);
        if (status.kind == ScopeStatusKind::unresolved) {
            error(
                    expr->main_token,
                    "unresolved symbol `",
                    expr->main_token->lexeme,
                    "`");
            return nullptr;
        }
        return status.stmt;
    }

    Stmt* assert_symbol_in_scope(Expr* expr) {
        assert(expr->kind == ExprKind::symbol);
        assert(expr->symbol.static_accessor.accessors.size() == 0);
        return this->assert_in_current_scope_rec(expr);
    }

    void symbol(Expr* expr) {
        expr->symbol.ref = this->assert_symbol_in_scope(expr);
    }

    void scoped_block(Expr* expr, bool create_new_scope) {
        Scope* scope = nullptr;
        if (create_new_scope) {
            scope_init(scope);
        }

        for (auto& stmt: expr->scoped_block.stmts) {
            this->pre_decl_stmt(stmt, true);
        }
        for (auto& stmt: expr->scoped_block.stmts) {
            this->stmt(stmt, false);
        }
        if (expr->scoped_block.value) {
            this->expr(expr->scoped_block.value);
        }

        if (create_new_scope) {
            scope_revert(scope);
        }
    }

    void unop(Expr* expr) {
        this->expr(expr->unop.child);
    }

    void binop(Expr* expr) {
        this->expr(expr->binop.left);
        this->expr(expr->binop.right);
    }

    void expr(Expr* expr) {
        switch (expr->kind) {
            case ExprKind::symbol: {
                this->symbol(expr);
            } break;

            case ExprKind::scoped_block: {
                this->scoped_block(expr, true);
            } break;

            case ExprKind::unop: {
                this->unop(expr);
            } break;

            case ExprKind::binop: {
                this->binop(expr);
            } break;
        }
    }

    void param(Stmt* stmt) {
        this->cpush_in_scope(stmt);
        this->type(stmt->param.type);
    }

    void function_header(FunctionHeader* header) {
        for (auto& param: header->params) {
            this->param(param);
        }
        this->type(header->ret_type);
    }

    void function(Stmt* stmt) {
        this->current_function = stmt;
        scope_new(scope);
        this->function_header(stmt->function.header);
        this->scoped_block(stmt->function.body, false);
        scope_revert(scope);
        this->current_function = nullptr;
    }

    void variable(Stmt* stmt, bool ignore_function_level_stmt) {
        if (!ignore_function_level_stmt) {
            this->cpush_in_scope(stmt);
        }

        if (!stmt->variable.type && !stmt->variable.initializer) {
            error(
                    stmt->main_token,
                    "type must be annotated or "
                    "an initializer must be provided");
            return;
        }

        if (stmt->variable.type) {
            this->type(stmt->variable.type);
        }
        if (stmt->variable.initializer) {
            this->expr(stmt->variable.initializer);
        }

        stmt->variable.function = this->current_function;
    }

    void expr_stmt(Stmt* stmt) {
        this->expr(stmt->expr_stmt.child);
    }

    void stmt(Stmt* stmt, bool ignore_function_level_stmt) {
        switch (stmt->kind) {
            case StmtKind::function: {
                this->function(stmt);
            } break;

            case StmtKind::variable: {
                this->variable(stmt, ignore_function_level_stmt);
            } break;

            case StmtKind::expr_stmt: {
                this->expr_stmt(stmt);
            } break;
        }
    }

    void run() {
        for (auto& stmt: this->srcfile->stmts) {
            this->pre_decl_stmt(stmt, false);
        }
        
        for (auto& stmt: this->srcfile->stmts) {
            this->stmt(stmt, true);
        }
    }
};
