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
        note(
                pair,
                "while matching `{}`...",
                pair->lexeme);
        fatal_error(parse_current(p), "unexpected end of file");
    }
}

Type* parse_atom_type(ParseContext* p) {
    Token* identifier = parse_expect_identifier(p, "type name");
    BuiltinTypeKind builtin_type_kind = 
        builtin_type_str_to_kind(identifier->lexeme);
    if (builtin_type_kind == BUILTIN_TYPE_KIND_NONE) {
        // TODO: implement custom types
        fatal_error(identifier, "internal error: custom types not implemented");
        assert(0);
    }
    return builtin_type_new(identifier, builtin_type_kind);
}

Type* parse_ptr_type(ParseContext* p) {
    if (parse_match(p, TOKEN_KIND_STAR)) {
        Token* star = parse_previous(p);
        bool constant = false;
        if (parse_match_keyword(p, "const")) {
            constant = true;
        }
        Type* child = parse_ptr_type(p);
        return ptr_type_new(
                star,
                constant,
                child);
    }
    return parse_atom_type(p);
}

Type* parse_type(ParseContext* p) {
    return parse_ptr_type(p);
}

Expr* parse_atom_expr(ParseContext* p);
StmtOrExpr parse_function_level_node(ParseContext* p);

Expr* parse_unop_expr(ParseContext* p) {
    if (parse_match(p, TOKEN_KIND_MINUS) ||
        parse_match(p, TOKEN_KIND_BANG) ||
        parse_match(p, TOKEN_KIND_AMP) ||
        parse_match(p, TOKEN_KIND_STAR)) {
        Token* op = parse_previous(p);
        Expr* child = parse_unop_expr(p);
        return unop_expr_new(child, op);
    }
    return parse_atom_expr(p);
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

Expr* parse_comparision_expr(ParseContext* p) {
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
    return parse_comparision_expr(p);
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
            stmts, 
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
            elseifbr,
            elsebr);
}

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
    return function_call_expr_new(callee, args, parse_previous(p));
}

Expr* parse_symbol_expr(ParseContext* p, Token* identifier) {
    return symbol_expr_new(identifier);
}

Expr* parse_integer_expr(ParseContext* p) {
    Token* integer = parse_previous(p);
    return integer_expr_new(integer, &integer->integer.val);
}

Expr* parse_atom_expr(ParseContext* p) {
    if (parse_match(p, TOKEN_KIND_INTEGER)) {
        return parse_integer_expr(p);
    }
    else if (parse_match_keyword(p, "true")) {
        return constant_expr_new(parse_previous(p), CONSTANT_KIND_BOOLEAN_TRUE);
    }
    else if (parse_match_keyword(p, "false")) {
        return constant_expr_new(parse_previous(p), CONSTANT_KIND_BOOLEAN_FALSE);
    }
    else if (parse_match_keyword(p, "null")) {
        return constant_expr_new(parse_previous(p), CONSTANT_KIND_NULL);
    }
    else if (parse_match(p, TOKEN_KIND_IDENTIFIER)) {
        Expr* symbol = parse_symbol_expr(p, parse_previous(p));
        if (parse_match(p, TOKEN_KIND_LPAREN)) {
            return parse_function_call_expr(p, symbol, parse_previous(p));
        }
        else {
            return symbol;
        }
    }
    else if (parse_match(p, TOKEN_KIND_LBRACE)) {
        return parse_block_expr(p, parse_previous(p), true);
    }
    else if (parse_match_keyword(p, "if")) {
        return parse_if_expr(p, parse_previous(p));
    }
    else {
        fatal_error(
                parse_current(p),
                "`{}` is invalid here",
                parse_current(p)->lexeme);
    }
    return null;
}

FunctionHeader* parse_function_header(ParseContext* p) {
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
            params,
            return_type);
}

Stmt* parse_variable_stmt(ParseContext* p) {
    bool constant = true;
    if (parse_match_keyword(p, "mut")) {
        constant = false;
    }

    Token* identifier = parse_expect_identifier(p, "variable name");
    Type* type = null;
    if (parse_match(p, TOKEN_KIND_COLON)) {
        type = parse_type(p);
    }

    Expr* initializer = null;
    if (parse_match(p, TOKEN_KIND_EQUAL)) {
        initializer = parse_expr(p);
    }

    parse_expect_semicolon(p);
    return variable_stmt_new(
            constant,
            identifier,
            type,
            initializer);
}

Stmt* parse_function_stmt(ParseContext* p, bool is_extern) {
    if (is_extern)
        parse_expect_keyword(p, "fn");

    FunctionHeader* header = parse_function_header(p);
    Token* lbrace = null;
    Expr* body = null;
    if (parse_match(p, TOKEN_KIND_LBRACE)) {
        lbrace = parse_previous(p);
        body = parse_block_expr(p, lbrace, true);
    } 
    else
        parse_expect_semicolon(p);
    return function_stmt_new(header, body, is_extern);
}

Stmt* parse_top_level_stmt(ParseContext* p, bool error_on_no_match) {
    if (parse_match_keyword(p, "extern")) {
        return parse_function_stmt(p, true);
    }  
    else if (parse_match_keyword(p, "fn")) {
        return parse_function_stmt(p, false);
    }
    else if (parse_match_keyword(p, "let")) {
        return parse_variable_stmt(p);
    }
    else if (error_on_no_match) {
        fatal_error(
                parse_current(p),
                "invalid token in top-level");
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

Stmt* parse_while_stmt(ParseContext* p, Token* while_keyword) {
    Expr* cond = parse_expr(p);
    Expr* body = parse_block_expr(p, parse_expect_lbrace(p), false);
    return while_stmt_new(
            while_keyword, 
            cond,
            body);
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

    if (parse_match_keyword(p, "let")) {
        result.stmt = parse_variable_stmt(p);
        return result;
    }
    else if (parse_match_keyword(p, "while")) {
        result.stmt = parse_while_stmt(p, parse_previous(p));
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

