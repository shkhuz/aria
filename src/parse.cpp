struct FunctionLevelNode {
    bool is_return_value;
    union {
        Stmt* stmt;
        Expr* expr;
    };
};

struct Parser {
    Srcfile* srcfile;
    size_t token_idx;
    bool error;

    Parser(Srcfile* srcfile)
        : srcfile(srcfile) {
        this->token_idx = 0;
        this->error = false;
    }

    Token* current() {
        if (this->token_idx < this->srcfile->tokens.size()) {
            return this->srcfile->tokens[this->token_idx];
        }
        assert(0);
        return nullptr;
    }

    Token* next() {
        if ((this->token_idx + 1) < this->srcfile->tokens.size()) {
            return this->srcfile->tokens[this->token_idx + 1];
        }
        assert(0);
        return nullptr;
    }

    Token* previous() {
        if (this->token_idx > 0) {
            return this->srcfile->tokens[this->token_idx - 1];
        }
        assert(0);
        return nullptr;
    }

    void goto_next_token() {
        if (this->token_idx < this->srcfile->tokens.size()) {
            this->token_idx++;
        }
    }

    void goto_previous_token() {
        if (this->token_idx > 0) {
            this->token_idx--;
        }
    }

    void check_eof(Token* pair) {
        if (this->current()->kind == TokenKind::eof) {
            msg::note(
                    pair,
                    "while matching `", 
                    pair->lexeme, 
                    "`...");
            msg::fatal_error(
                    current(),
                    "unexpected end of file");
        }
    }

    bool match(TokenKind kind) {
        if (this->current()->kind == kind) {
            this->goto_next_token();
            return true;
        }
        return false;
    }

    bool match_keyword(const std::string& keyword) {
        if (this->current()->kind == TokenKind::keyword &&
            this->current()->lexeme == keyword) {
            this->goto_next_token();
            return true;
        }
        return false;
    }

    Token* expect_keyword(const std::string& keyword) {
        if (this->match_keyword(keyword)) {
            return this->previous();
        }
        msg::fatal_error(
                this->current(),
                "expected keyword `", 
                keyword, 
                "`, got `", 
                this->current()->lexeme, 
                "`");
        return nullptr;
    }

    template<typename T>
    Token* expect(
            TokenKind kind, 
            T first) {
        if (!this->match(kind)) {
            msg::fatal_error(
                    this->current(),
                    "expected ", 
                    first,
                    ", got `", 
                    this->current()->lexeme, 
                    "`");
            return nullptr;
        }
        return this->previous();
    }

    Token* expect_identifier(const std::string& expected) {
        return this->expect(
                TokenKind::identifier,
                expected);
    }

    Token* expect_lparen() {
        return this->expect(
                TokenKind::lparen,
                "`(`");
    }

    Token* expect_lbrace() {
        return this->expect(
                TokenKind::lbrace,
                "`{`");
    }

    Token* expect_colon() {
        return this->expect(
                TokenKind::colon,
                "`:`");
    }

    Token* expect_semicolon() {
        return this->expect(
                TokenKind::semicolon,
                "`;`");
    }

    Token* expect_comma() {
        return this->expect(
                TokenKind::comma,
                "`,`");
    }

    Type** atom_type() {
        Token* identifier = this->expect_identifier("type name");
        BuiltinTypeKind builtin_kind = 
            builtin_type::str_to_kind(identifier->lexeme);
        if (builtin_kind == BuiltinTypeKind::none) {
            // TODO: custom types
            assert(0);
        } else {
            return builtin_type_new(identifier, builtin_kind);
        }
    }

    Type** ptr_type() {
        if (this->match(TokenKind::star)) {
            Token* star = this->previous();

            bool constant = false;
            if (this->match_keyword("const")) {
                constant = true;
            }

            return ptr_type_new(
                    star, 
                    constant, 
                    this->ptr_type());
        }
        return this->atom_type();
    }

    Type** type() {
        return this->ptr_type();
    }

    Expr* scoped_block(Token* lbrace) {
        std::vector<Stmt*> stmts;
        Expr* return_value = nullptr;
        while (!this->match(TokenKind::rbrace)) {
            this->check_eof(lbrace);
            FunctionLevelNode meta = this->function_level_node();
            if (meta.is_return_value) return_value = meta.expr;
            else stmts.push_back(meta.stmt);
        }
        return scoped_block_new(
                lbrace,
                stmts,
                return_value
        );
    }

    StaticAccessor static_accessor() {
        std::vector<Token*> accessors;
        bool from_global_scope = false;
        Token* head = nullptr;

        while (true) {
            if (this->current()->kind == TokenKind::identifier &&
                this->next()->kind == TokenKind::double_colon) {
                if (!head) head = this->current();
                accessors.push_back(this->current());
                this->goto_next_token();
                this->goto_next_token();
            } else break;
        }

        return StaticAccessor {
            accessors,
            from_global_scope,
            head,
        };
    }
    
    Expr* number() {
        Token* number = this->previous();
        return number_new(number, number->number.val);
    }

    Expr* symbol() {
        StaticAccessor static_accessor = this->static_accessor();
        Token* identifier = this->expect_identifier("identifier");
        BuiltinTypeKind kind = builtin_type::str_to_kind(identifier->lexeme);
        return symbol_new(static_accessor, identifier);
    }

    Expr* atom_expr() {
        if (this->match(TokenKind::number)) {
            return this->number();
        } else if (this->current()->kind == TokenKind::identifier) {
            return this->symbol();
        } else if (this->match(TokenKind::lbrace)) {
            return this->scoped_block(this->previous());
        } else {
            msg::fatal_error(
                    this->current(),
                    "`", 
                    this->current()->lexeme, 
                    "` is invalid here");
        }
        return nullptr;
    }

    Expr* unop_expr() {
        if (this->match(TokenKind::minus)) {
            Token* op = this->previous();
            Expr* child = this->unop_expr();
            return unop_new(child, op);
        }
        return this->atom_expr();
    }

    Expr* binop_expr() {
        Expr* left = this->unop_expr();
        while (this->match(TokenKind::plus) || this->match(TokenKind::minus)) {
            Token* op = this->previous();
            left = binop_new(
                    left, 
                    this->unop_expr(),
                    op);
        }
        return left;
    }

    Expr* expr() {
        return binop_expr();
    }

    Stmt* expr_stmt(Expr* expr) {
        if (!expr) return {};
        if (expr->kind != ExprKind::scoped_block) {
            this->expect_semicolon();
        }
        return expr_stmt_new(expr);
    }

    FunctionLevelNode function_level_node() {
        FunctionLevelNode result {
            false,
            { nullptr },
        };

        if (this->match_keyword("return")) {
            Token* keyword = this->previous();
            Expr* child = nullptr;
            if (this->current()->kind != TokenKind::semicolon) {
                child = this->expr();
            }
            this->expect_semicolon();
            result.stmt = return_new(keyword, child);
            return result;
        } else if (this->match_keyword("let")) {
            result.stmt = this->variable();
            return result;
        }

        Expr* expr = this->expr();
        if (this->current()->kind == TokenKind::rbrace) {
            result.is_return_value = true;
            result.expr = expr;
            return result;
        }
        result.stmt = this->expr_stmt(expr);
        return result;
    }

    FunctionHeader* function_header() {
        Token* identifier = this->expect_identifier("procedure name");
        Token* lparen = this->expect_lparen();

        std::vector<Stmt*> params;
        while (!this->match(TokenKind::rparen)) {
            this->check_eof(lparen);
            Token* param_identifier = this->expect_identifier("parameter");
            this->expect_colon();
            Type** param_type = this->type();
            params.push_back(param_new(
                    param_identifier,
                    param_type
            ));

            if (this->current()->kind != TokenKind::rparen) {
                this->expect_comma();
            }
        }

        Type** ret_type = builtin_type_placeholders.void_kind;
        if (this->current()->kind != TokenKind::lbrace) {
            ret_type = this->type();
        }
        return function_header_new(
            identifier,
            params,
            ret_type);
    }

    Stmt* variable() {
        bool constant = true;
        if (this->match_keyword("mut")) {
            constant = false;
        }

        Token* identifier = this->expect_identifier("variable name");
        Type** type = nullptr;
        if (this->match(TokenKind::colon)) {
            type = this->type();
        }

        Expr* initializer = nullptr;
        if (this->match(TokenKind::equal)) {
            initializer = this->expr();
        }

        this->expect_semicolon();
        return variable_new(
                constant, 
                identifier,
                type,
                initializer
        );
    }

    Stmt* top_level_stmt(bool error_on_no_match) {
        if (this->match_keyword("fn")) {
            FunctionHeader* header = this->function_header();
            Token* lbrace = this->expect_lbrace();
            Expr* body = this->scoped_block(lbrace);
            return function_new(header, body);
        } else if (this->match_keyword("let")) {
            return this->variable();
        } else if (error_on_no_match) {
            msg::fatal_error(
                    this->current(),
                    "invalid token in top-level");
        }

        if (error_on_no_match) assert(0);
        return nullptr;
    }

    void run() {
        while (this->current()->kind != TokenKind::eof) {
            Stmt* stmt = this->top_level_stmt(true);
            if (stmt) this->srcfile->stmts.push_back(stmt);
        }
    }
};
