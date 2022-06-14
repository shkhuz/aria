struct ParseContext {
    Srcfile* srcfile;
    size_t token_idx;
    bool error;
};

struct StmtOrExpr {
    bool is_stmt;
    union {
        Stmt* stmt;
        Expr* expr;
    };
};

Token* parse_next(ParseContext* p) {
    if ((p->token_idx+1) < p->srcfile->tokens.size()) {
        return p->srcfile->tokens[p->token_idx+1];
    }
    assert(0);
    return null;
}

Token* parse_previous(ParseContext* p) {
    if (p->token_idx > 0) {
        return p->srcfile->tokens[p->token_idx-1];
    }
    assert(0);
    return null;
}

Token* parse_current(ParseContext* p) {
    if (p->token_idx < p->srcfile->tokens.size()) {
        return p->srcfile->tokens[p->token_idx];
    }
    assert(0);
    return null;
}

void parse_goto_next_token(ParseContext* p) {
    if (p->token_idx < p->srcfile->tokens.size()) {
        p->token_idx++;
    }
}

void parse_goto_previous_token(ParseContext* p) {
    if (p->token_idx > 0) {
        p->token_idx--;
    }
}

bool parse_match(ParseContext* p, TokenKind kind) {
    if (parse_current(p)->kind == kind) {
        parse_goto_next_token(p);
        return true;
    }
    return false;
}

Token* parse_expect(ParseContext* p, TokenKind kind, const std::string& expect) {
    if (parse_match(p, kind)) {
        return parse_previous(p);
    }

    fatal_error(
            parse_current(p),
            "expected {}, got `{}`",
            expect,
            parse_current(p)->lexeme);
    return null;
}

Token* parse_expect_identifier(ParseContext* p, const std::string& expect) {
    return parse_expect(p, TOKEN_KIND_IDENTIFIER, expect);
}

Token* parse_expect_lbrace(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_LBRACE,
            "`{`");
}

Token* parse_expect_rbrace(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_RBRACE,
            "`}`");
}

Token* parse_expect_lbrack(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_LBRACK,
            "`[`");
}

Token* parse_expect_rbrack(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_RBRACK,
            "`]`");
}

Token* parse_expect_lparen(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_LPAREN,
            "`(`");
}

Token* parse_expect_rparen(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_RPAREN,
            "`)`");
}

Token* parse_expect_colon(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_COLON,
            "`:`");
}

Token* parse_expect_semicolon(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_SEMICOLON,
            "`;`");
}

Token* parse_expect_comma(ParseContext* p) {
    return parse_expect(
            p,
            TOKEN_KIND_COMMA,
            "`,`");
}

bool parse_match_keyword(ParseContext* p, const std::string& keyword) {
    if (parse_current(p)->kind == TOKEN_KIND_KEYWORD && 
        parse_current(p)->lexeme == keyword) {
        parse_goto_next_token(p);
        return true;
    }
    return false;
}

Token* parse_expect_keyword(ParseContext* p, const std::string& keyword) {
    if (parse_match_keyword(p, keyword)) {
        return parse_previous(p);
    }

    fatal_error(
            parse_current(p), 
            "expected keyword `{}`, got `{}`",
            keyword,
            parse_current(p)->lexeme);
    return null;
}

void parse_check_eof(ParseContext* p, Token* pair) {
    if (parse_current(p)->kind == TOKEN_KIND_EOF) {
        error(parse_current(p), "unexpected end of file");
        note(
                pair,
                "while matching `{}`",
                pair->lexeme);
        terminate_compilation();
    }
}

Expr* parse_expr(ParseContext* p);

Type* parse_type(ParseContext* p) {
    if (parse_match(p, TOKEN_KIND_STAR)) {
        Token* star = parse_previous(p);
        bool constant = false;
        if (parse_match_keyword(p, "const")) {
            constant = true;
        }
        Type* child = parse_type(p);
        return ptr_type_new(
                star,
                constant,
                child);
    }
    else if (parse_match(p, TOKEN_KIND_LBRACK)) {
        Token* lbrack = parse_previous(p);
        if (parse_match(p, TOKEN_KIND_RBRACK)) {
            bool constant = false;
            if (parse_match_keyword(p, "const")) {
                constant = true;
            }
            Type* child = parse_type(p);
            return slice_type_new(lbrack, constant, child);
        }
        else {
            Expr* len = parse_expr(p);
            if (len->kind == EXPR_KIND_INTEGER) {
                parse_expect_rbrack(p);
                Type* elem_type = parse_type(p);
                return array_type_new(len, elem_type, lbrack);
            }
            else {
                fatal_error(
                        len->main_token,
                        "expected compile-time number literal for array length");
            }
        }
    }
    else {
        Token* identifier = parse_expect_identifier(p, "type name");
        BuiltinTypeKind builtin_type_kind = 
            builtin_type_str_to_kind(identifier->lexeme);
        if (builtin_type_kind == BUILTIN_TYPE_KIND_NONE) {
            return custom_type_new(identifier);
        }
        else {
            return builtin_type_new(identifier, builtin_type_kind);
        }
    }
    return null;
}

Expr* parse_atom_expr(ParseContext* p);
StmtOrExpr parse_function_level_node(ParseContext* p);

Expr* parse_function_call_expr(
        ParseContext* p, 
        Expr* callee, 
        Token* lparen) {
    std::vector<Expr*> args;
    while (!parse_match(p, TOKEN_KIND_RPAREN)) {
        parse_check_eof(p, lparen);
        Expr* arg = parse_expr(p);
        args.push_back(arg);
        if (parse_current(p)->kind != TOKEN_KIND_RPAREN) {
            parse_expect_comma(p);
        }
    }
    return function_call_expr_new(callee, std::move(args), parse_previous(p));
}

Expr* parse_suffix_expr(ParseContext* p) {
    Expr* left = parse_atom_expr(p);
    while (parse_current(p)->kind == TOKEN_KIND_LPAREN ||
           parse_current(p)->kind == TOKEN_KIND_LBRACK ||
           parse_current(p)->kind == TOKEN_KIND_DOT) {
        if (parse_match(p, TOKEN_KIND_LPAREN)) {
            left = parse_function_call_expr(p, left, parse_previous(p));
        }
        else if (parse_match(p, TOKEN_KIND_LBRACK)) {
            Token* lbrack = parse_previous(p);
            Expr* idx = parse_expr(p);
            // TODO: better error messages
            parse_expect_rbrack(p);
            left = index_expr_new(left, idx, lbrack);
        }
        else if (parse_match(p, TOKEN_KIND_DOT)) {
            Token* dot = parse_previous(p);
            Token* field = parse_expect_identifier(p, "field name");
            left = field_access_expr_new(left, field, dot);
        }
    }
    return left;
}

Expr* parse_unop_expr(ParseContext* p) {
    if (parse_match(p, TOKEN_KIND_MINUS) ||
        parse_match(p, TOKEN_KIND_BANG) ||
        parse_match(p, TOKEN_KIND_AMP) ||
        parse_match(p, TOKEN_KIND_STAR)) {
        Token* op = parse_previous(p);
        Expr* child = parse_unop_expr(p);
        return unop_expr_new(child, op);
    }
    return parse_suffix_expr(p);
}

Expr* parse_cast_expr(ParseContext* p) {
    Expr* left = parse_unop_expr(p);
    while (parse_match_keyword(p, "as")) {
        Token* op = parse_previous(p);
        Type* to = parse_type(p);
        left = cast_expr_new(left, to, op);
    }
    return left;
}

Expr* parse_binop_arith_term_expr(ParseContext* p) {
    Expr* left = parse_cast_expr(p);
    while (parse_match(p, TOKEN_KIND_PLUS) ||
           parse_match(p, TOKEN_KIND_MINUS)) {
        Token* op = parse_previous(p);
        Expr* right = parse_cast_expr(p);
        left = binop_expr_new(left, right, op);
    }
    return left;
}

Expr* parse_comparison_expr(ParseContext* p) {
    Expr* left = parse_binop_arith_term_expr(p);
    while (parse_match(p, TOKEN_KIND_DOUBLE_EQUAL) ||
           parse_match(p, TOKEN_KIND_BANG_EQUAL) ||
           parse_match(p, TOKEN_KIND_LANGBR) ||
           parse_match(p, TOKEN_KIND_LANGBR_EQUAL) ||
           parse_match(p, TOKEN_KIND_RANGBR) ||
           parse_match(p, TOKEN_KIND_RANGBR_EQUAL)) {
        Token* op = parse_previous(p);
        Expr* right = parse_binop_arith_term_expr(p);
        left = binop_expr_new(left, right, op);
    }
    return left;
}

Expr* parse_expr(ParseContext* p) {
    return parse_comparison_expr(p);
}

Expr* parse_block_expr(ParseContext* p, Token* lbrace, bool value_req) {
    std::vector<Stmt*> stmts;
    Expr* value = null;
    while (!parse_match(p, TOKEN_KIND_RBRACE)) {
        parse_check_eof(p, lbrace);
        StmtOrExpr meta = parse_function_level_node(p);
        if (meta.is_stmt) {
            stmts.push_back(meta.stmt);
        }
        else if (!value_req) {
            fatal_error(
                    meta.expr->main_token,
                    "this block cannot return a value");
        }
        else {
            value = meta.expr;
        }
    }
    Token* rbrace = parse_previous(p);
    return block_expr_new(
            std::move(stmts), 
            value, 
            lbrace, 
            rbrace);
}

IfBranch* parse_if_branch(ParseContext* p, IfBranchKind kind) {
    Expr* cond = null;
    if (kind != IF_BRANCH_ELSE) {
        cond = parse_expr(p);
    }

    Expr* body = parse_block_expr(p, parse_expect_lbrace(p), true);
    return if_branch_new(
            cond,
            body,
            kind);
}

Expr* parse_if_expr(ParseContext* p, Token* if_keyword) {
    IfBranch* ifbr = parse_if_branch(p, IF_BRANCH_IF);
    std::vector<IfBranch*> elseifbr;
    IfBranch* elsebr = null;

    bool elsebr_exists = false;
    for (;;) {
        if (parse_match_keyword(p, "else")) {
            if (parse_match_keyword(p, "if"))
                elseifbr.push_back(parse_if_branch(p, IF_BRANCH_ELSEIF));
            else {
                elsebr_exists = true;
                break;
            }
        }
        else 
            break;
    }

    if (elsebr_exists) 
        elsebr = parse_if_branch(p, IF_BRANCH_ELSE);

    return if_expr_new(
            if_keyword,
            ifbr,
            std::move(elseifbr),
            elsebr);
}

Expr* parse_while_expr(ParseContext* p, Token* while_keyword) {
    Expr* cond = parse_expr(p);
    Expr* body = parse_block_expr(p, parse_expect_lbrace(p), false);
    return while_expr_new(
            while_keyword, 
            cond,
            body);
}

Expr* parse_symbol_expr(ParseContext* p, Token* identifier) {
    return symbol_expr_new(identifier);
}

Expr* parse_integer_expr(ParseContext* p) {
    Token* integer = parse_previous(p);
    return integer_expr_new(integer, &integer->integer.val);
}

Expr* parse_atom_expr(ParseContext* p) {
    Expr* result = null;
    if (parse_match(p, TOKEN_KIND_INTEGER)) {
        result = parse_integer_expr(p);
    }
    else if (parse_match_keyword(p, "true")) {
        result = constant_expr_new(parse_previous(p), CONSTANT_KIND_BOOLEAN_TRUE);
    }
    else if (parse_match_keyword(p, "false")) {
        result = constant_expr_new(parse_previous(p), CONSTANT_KIND_BOOLEAN_FALSE);
    }
    else if (parse_match_keyword(p, "null")) {
        result = constant_expr_new(parse_previous(p), CONSTANT_KIND_NULL);
    }
    else if (parse_match(p, TOKEN_KIND_LBRACK)) {
        Token* lbrack = parse_previous(p);
        std::vector<Expr*> elems;
        while (!parse_match(p, TOKEN_KIND_RBRACK)) {
            parse_check_eof(p, lbrack);
            Expr* elem = parse_expr(p);
            elems.push_back(elem);
            if (parse_current(p)->kind != TOKEN_KIND_RBRACK) {
                parse_expect_comma(p);
            }
        }
        result = array_literal_expr_new(std::move(elems), lbrack);
    }
    else if (parse_match(p, TOKEN_KIND_IDENTIFIER)) {
        result = parse_symbol_expr(p, parse_previous(p));
    }
    else if (parse_match(p, TOKEN_KIND_LBRACE)) {
        result = parse_block_expr(p, parse_previous(p), true);
    }
    else if (parse_match_keyword(p, "if")) {
        result = parse_if_expr(p, parse_previous(p));
    }
    else if (parse_match_keyword(p, "while")) {
        result = parse_while_expr(p, parse_previous(p));
    }
    else {
        fatal_error(
                parse_current(p),
                "`{}` is invalid here",
                parse_current(p)->lexeme);
    }

    return result;
}

FunctionHeader* parse_function_header(ParseContext* p, bool is_extern) {
    Token* identifier = parse_expect_identifier(p, "function name");
    Token* lparen = parse_expect_lparen(p);

    std::vector<Stmt*> params;
    size_t param_idx = 0;
    while (!parse_match(p, TOKEN_KIND_RPAREN)) {
        parse_check_eof(p, lparen);
        Token* param_identifier = parse_expect_identifier(p, "parameter");
        parse_expect_colon(p);
        Type* param_type = parse_type(p);
        params.push_back(
            param_stmt_new(param_identifier, param_type, param_idx));

        if (parse_current(p)->kind != TOKEN_KIND_RPAREN) {
            parse_expect_comma(p);
        }
        param_idx++;
    }

    Type* return_type = builtin_type_placeholders.void_kind;
    if (parse_current(p)->kind != TOKEN_KIND_LBRACE &&
        parse_current(p)->kind != TOKEN_KIND_SEMICOLON) {
        return_type = parse_type(p);
    }

    return function_header_new(
            identifier,
            std::move(params),
            return_type,
            is_extern);
}

Stmt* parse_variable_stmt(ParseContext* p, bool is_extern, bool constant) {
    Token* identifier = parse_expect_identifier(p, "variable name");
    Type* type = null;
    if (parse_match(p, TOKEN_KIND_COLON)) {
        type = parse_type(p);
    }

    Expr* initializer = null;
    if (parse_match(p, TOKEN_KIND_EQUAL)) {
        if (is_extern) {
            fatal_error(
                    parse_previous(p),
                    "extern declarations cannot be initialized");
        }
        else {
            initializer = parse_expr(p);
        }
    }
    if (is_extern && !type) {
        fatal_error(
                parse_current(p),
                "type annotations are mandatory for extern declarations");
    }

    parse_expect_semicolon(p);
    return variable_stmt_new(
            constant,
            is_extern,
            identifier,
            type,
            initializer);
}

Stmt* parse_function_stmt(ParseContext* p, bool is_extern) {
    FunctionHeader* header = parse_function_header(p, is_extern);
    Token* lbrace = null;
    Expr* body = null;
    if (parse_match(p, TOKEN_KIND_LBRACE)) {
        lbrace = parse_previous(p);
        body = parse_block_expr(p, lbrace, true);
    } 
    else
        parse_expect_semicolon(p);
    return function_stmt_new(header, body);
}

Stmt* parse_top_level_stmt(ParseContext* p, bool error_on_no_match) {
    if (parse_match_keyword(p, "extern")) {
        if (parse_match_keyword(p, "fn")) {
            return parse_function_stmt(p, true);
        }
        else if (parse_match_keyword(p, "const")) {
            return parse_variable_stmt(p, true, true);
        }
        else if (parse_match_keyword(p, "var")) {
            return parse_variable_stmt(p, true, false);
        }
        else {
            fatal_error(
                    parse_current(p),
                    "expected `fn`, `const` or `var` after `extern`");
        }
    }  
    else if (parse_match_keyword(p, "struct")) {
        Token* identifier = parse_expect_identifier(p, "struct name");
        std::vector<Stmt*> fields;
        Token* lbrace = parse_expect_lbrace(p);
        size_t field_idx = 0;
        while (!parse_match(p, TOKEN_KIND_RBRACE)) {
            parse_check_eof(p, lbrace);
            Token* field_identifier = parse_expect_identifier(p, "field name");
            parse_expect_colon(p);
            Type* field_type = parse_type(p);
            fields.push_back(field_stmt_new(
                        field_identifier, 
                        field_type,
                        field_idx));
            if (parse_current(p)->kind != TOKEN_KIND_RBRACE) {
                parse_expect_comma(p);
            }
            field_idx++;
        }
        return struct_stmt_new(identifier, std::move(fields), false);
    }
    else if (parse_match_keyword(p, "fn")) {
        return parse_function_stmt(p, false);
    }
    else if (parse_match_keyword(p, "const")) {
        return parse_variable_stmt(p, false, true);
    }
    else if (parse_match_keyword(p, "var")) {
        return parse_variable_stmt(p, false, false);
    }
    else if (error_on_no_match) {
        error(
                parse_current(p),
                "invalid token in top-level");
        addinfo("expected `fn`, `struct`, `const`, `var`");
        terminate_compilation();
    }
    return null;
}

void parse(ParseContext* p) {
    p->token_idx = 0;
    p->error = false;

    while (parse_current(p)->kind != TOKEN_KIND_EOF) {
        Stmt* stmt = parse_top_level_stmt(p, true);
        if (stmt) p->srcfile->stmts.push_back(stmt);
    }
}

Stmt* parse_assign_stmt(
        ParseContext* p,
        Expr* left,
        Token* op) {
    Expr* right = parse_expr(p);
    parse_expect_semicolon(p);
    return assign_stmt_new(left, right, op);
}

Stmt* parse_expr_stmt(ParseContext* p, Expr* expr) {
    assert(expr);
    if (expr->kind != EXPR_KIND_BLOCK &&
        expr->kind != EXPR_KIND_IF) {
        parse_expect_semicolon(p);
    }
    return expr_stmt_new(expr);
}

StmtOrExpr parse_function_level_node(ParseContext* p) {
    StmtOrExpr result;
    result.is_stmt = true;
    result.stmt = null;

    if (parse_match_keyword(p, "const")) {
        result.stmt = parse_variable_stmt(p, false, true);
        return result;
    }
    else if (parse_match_keyword(p, "var")) {
        result.stmt = parse_variable_stmt(p, false, false);
        return result;
    }

    Expr* expr = parse_expr(p);
    if (parse_current(p)->kind == TOKEN_KIND_RBRACE) {
        result.is_stmt = false;
        result.expr = expr;
        return result;
    }
    else if (parse_match(p, TOKEN_KIND_EQUAL)) {
        result.stmt = parse_assign_stmt(
                p,
                expr,
                parse_previous(p));
        return result;
    }
    result.stmt = parse_expr_stmt(p, expr);
    return result;
}

