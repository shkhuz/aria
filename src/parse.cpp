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

    Token* expect_comma() {
        return this->expect(
                TokenKind::comma,
                "`,`");
    }

    Type* atom_type() {
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

    Type* ptr_type() {
        if (this->match(TokenKind::star)) {
            Token* star = this->previous();

            bool constant = false;
            if (match_keyword("const")) {
                constant = true;
            }

            return ptr_type_new(
                    star, 
                    constant, 
                    this->ptr_type());
        }
        return this->atom_type();
    }

    Type* type() {
        return this->ptr_type();
    }

    ScopedBlock block(Token* lbrace) {
        while (!this->match(TokenKind::rbrace)) {
            this->check_eof(lbrace);
        }
        return ScopedBlock {
            {},
            nullptr,
        };
    }

    void function_header(FunctionHeader* header) {
        header->identifier = this->expect_identifier("procedure name");

        Token* lparen = this->expect_lparen();
        while (!this->match(TokenKind::rparen)) {
            this->check_eof(lparen);
            Token* param_identifier = this->expect_identifier("parameter");
            this->expect_colon();
            Type* param_type = this->type();
            header->params.push_back(new Param {
                    param_identifier,
                    param_type,
            });

            if (this->current()->kind != TokenKind::rparen) {
                this->expect_comma();
            }
        }

        header->ret_type = builtin_type_placeholders.void_kind;
        if (this->current()->kind != TokenKind::lbrace) {
            header->ret_type = this->type();
        }
    }

    Stmt* top_level_stmt(bool error_on_no_match) {
        if (this->match_keyword("fn")) {
            FunctionHeader header {};
            this->function_header(&header);
            return function_new(header, this->block(this->expect_lbrace()));
        }
        return nullptr;
    }

    void run() {
        while (this->current()->kind != TokenKind::eof) {
            Stmt* stmt = this->top_level_stmt(true);
            if (stmt) this->srcfile->stmts.push_back(stmt);
        }
    }
};
