typedef struct {
    SrcFile* srcfile;
    u64 token_idx;
    bool error;
} Parser;

Node* parser_top_level_node(Parser* self, bool error_on_no_match);
Node* parser_procedure_level_node(Parser* self);
Node* parser_expr(Parser* self);

Token* parser_current(Parser* self) {
    if (self->token_idx < buf_len(self->srcfile->tokens)) {
        return self->srcfile->tokens[self->token_idx];
    }
    assert(0);
    return null;
}

Token* parser_next(Parser* self) {
    if ((self->token_idx + 1) < buf_len(self->srcfile->tokens)) {
        return self->srcfile->tokens[self->token_idx + 1];
    }
    assert(0);
    return null;
}

Token* parser_previous(Parser* self) {
    if (self->token_idx > 0) {
        return self->srcfile->tokens[self->token_idx - 1];
    }
    assert(0);
    return null;
}

void parser_goto_next_token(Parser* self) {
    if (self->token_idx < buf_len(self->srcfile->tokens)) {
        self->token_idx++;
    }
}

void parser_goto_previous_token(Parser* self) {
    if (self->token_idx > 0) {
        self->token_idx--;
    }
}

// Checks EOF.
// Make sure to call this in a loop.
void parser_check_eof(Parser* self, Token* pair) {
    if (parser_current(self)->kind == TOKEN_KIND_EOF) {
        note_token(
                pair,
                "...while matching `%s`...",
                pair->lexeme);
        fatal_error_token(
                parser_current(self),
                "unexpected end of file");
    }
}

bool parser_match_token(Parser* self, TokenKind kind) {
    if (parser_current(self)->kind == kind) {
        parser_goto_next_token(self);
        return true;
    }
    return false;
}

bool parser_match_keyword(Parser* self, char* keyword) {
    if (parser_current(self)->kind == TOKEN_KIND_KEYWORD) {
        if (stri(parser_current(self)->lexeme) == stri(keyword)) {
            parser_goto_next_token(self);
            return true;
        }
    }
    return false;
}

Token* parser_expect(Parser* self, TokenKind kind, char* fmt, ...) {
    if (!parser_match_token(self, kind)) {
        va_list ap;
        va_start(ap, fmt);
        fatal_verror_token(parser_current(self), fmt, ap);
        va_end(ap);
        return null;
    }
    return parser_previous(self);
}

#define parser_expect_identifier(self, fmt) \
    parser_expect( \
            self, \
            TOKEN_KIND_IDENTIFIER, \
            fmt, \
            parser_current(self)->lexeme)

#define parser_expect_lparen(self) \
    parser_expect( \
            self, \
            TOKEN_KIND_LPAREN, \
            "expected `(`, got `%s`", \
            parser_current(self)->lexeme)

#define parser_expect_lbrace(self) \
    parser_expect( \
            self, \
            TOKEN_KIND_LBRACE, \
            "expected `{`, got `%s`", \
            parser_current(self)->lexeme)

#define parser_expect_semicolon(self) \
    parser_expect( \
            self, \
            TOKEN_KIND_SEMICOLON, \
            "expected `;`, got `%s`", \
            parser_current(self)->lexeme)

#define parser_expect_comma(self) \
    parser_expect( \
            self, \
            TOKEN_KIND_COMMA, \
            "expected `,`, got `%s`", \
            parser_current(self)->lexeme)

Node* parser_type_base(Parser* self) {
    Token* identifier = 
        parser_expect_identifier(self, "expected type identifier, got `%s`");
    return node_type_base_new(node_symbol_new(identifier));
}

Node* parser_type_ptr(Parser* self) {
    if (parser_match_token(self, TOKEN_KIND_STAR)) {
        return node_type_ptr_new(
                parser_previous(self), 
                parser_type_ptr(self));
    }
    return parser_type_base(self);
}

Node* parser_type(Parser* self) {
    return parser_type_ptr(self);
}

Node* parser_symbol(Parser* self) {
    Token* identifier = parser_expect_identifier(
            self,
            "expected identifier, got `%s`");
    return node_symbol_new(identifier);
}

Node* parser_procedure_call(
        Parser* self, 
        Node* callee, 
        Token* lparen) {
    Node** args = null;
    while (!parser_match_token(self, TOKEN_KIND_RPAREN)) {
        parser_check_eof(self, lparen);
        Node* arg = parser_expr(self);
        buf_push(args, arg);
        if (parser_current(self)->kind != TOKEN_KIND_RPAREN) {
            parser_expect_comma(self);
        }
    }

    return node_procedure_call_new(
            callee,
            args,
            parser_previous(self));
}

Node* parser_block(Parser* self, Token* lbrace) {
    Node** nodes = null;
    while (!parser_match_token(self, TOKEN_KIND_RBRACE)) {
        parser_check_eof(self, lbrace);
        Node* node = parser_procedure_level_node(self);
        buf_push(nodes, node);
    }
    Token* rbrace = parser_previous(self);
    return node_block_new(lbrace, nodes, rbrace);
}

Node* parser_expr_atom(Parser* self) {
    // TODO: add block expression here
    if (parser_current(self)->kind == TOKEN_KIND_IDENTIFIER) {
        Node* symbol = parser_symbol(self);
        if (parser_match_token(self, TOKEN_KIND_LPAREN)) {
            return parser_procedure_call(self, symbol, parser_previous(self));
        } else {
            return symbol;
        }
    } else if (parser_match_token(self, TOKEN_KIND_NUMBER)) {
        return node_number_new(parser_previous(self));
    }
    return null;
}

Node* parser_expr(Parser* self) {
    return parser_expr_atom(self);
}

Node* parser_expr_stmt(Parser* self) {
    Node* node = parser_expr(self);
    Token* semicolon = parser_expect_semicolon(self);
    return node_expr_stmt_new(node, semicolon);
}

Node* parser_variable_decl(Parser* self, Token* keyword) {
    bool mut = false;
    if (parser_match_keyword(self, "mut")) {
        mut = true;
    }
    
    Token* identifier = 
        parser_expect_identifier(self, "expected variable name, got `%s`");

    Node* type = null;
    if (parser_match_token(self, TOKEN_KIND_COLON)) {
        type = parser_type(self);
    }

    Node* initializer = null;
    if (parser_match_token(self, TOKEN_KIND_EQUAL)) {
        initializer = parser_expr(self);
    }

    Token* semicolon = parser_expect_semicolon(self);
    return node_variable_decl_new(
            keyword, 
            mut, 
            identifier, 
            type, 
            initializer,
            semicolon);
}

Node* parser_procedure_decl(Parser* self, Token* keyword) {
    Token* identifier = 
        parser_expect_identifier(self, "expected procedure name, got `%s`");

    Node** params = null;
    Token* lparen = parser_expect_lparen(self);
    while (!parser_match_token(self, TOKEN_KIND_RPAREN)) {
        parser_check_eof(self, lparen);
        parser_goto_next_token(self);
    }

    Node* type = void_type;
    if (parser_current(self)->kind != TOKEN_KIND_LBRACE) {
        type = parser_type(self);
    } 

    Node* body = parser_block(self, parser_expect_lbrace(self));

    return node_procedure_decl_new(
            keyword,
            identifier,
            params,
            type,
            body);
}

Node* parser_top_level_node(Parser* self, bool error_on_no_match) {
    if (parser_match_keyword(self, PROCEDURE_DECL_KEYWORD)) {
        return parser_procedure_decl(self, parser_previous(self));
    } else if (parser_match_keyword(self, VARIABLE_DECL_KEYWORD)) {
        return parser_variable_decl(self, parser_previous(self));
    } else {
        if (error_on_no_match) {
            fatal_error_token(
                    parser_current(self),
                    "invalid token in top-level");
        }
    }

    if (error_on_no_match) {
        assert(0);
    }
    return null;
}

Node* parser_procedure_level_node(Parser* self) {
    Node* node = parser_top_level_node(self, false);
    if (node) {
        return node;
    }

    return parser_expr_stmt(self);
}

void parser_init(Parser* self, SrcFile* srcfile) {
    self->srcfile = srcfile;
    self->srcfile->nodes = null;
    self->token_idx = 0;
    self->error = false;
}

void parser_parse(Parser* self) {
    while (parser_current(self)->kind != TOKEN_KIND_EOF) {
        Node* node = parser_top_level_node(self, true);
        buf_push(self->srcfile->nodes, node);
    }
}
