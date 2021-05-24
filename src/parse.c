typedef struct {
    SrcFile* srcfile;
    u64 token_idx;
    bool in_function;
    bool error;
} Parser;

Node* parser_top_level_node(Parser* self);

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

Node* parser_match_type(Parser* self) {
    if (parser_match_token(self, TOKEN_KIND_IDENTIFIER)) {
        return node_type_identifier_new(parser_previous(self));
    }
    return null;
}

Node* parser_expect_type(Parser* self) {
    Node* type = parser_match_type(self);
    if (!type) {
        fatal_error_token(
                parser_current(self),
                "expected type, got `%s`",
                parser_current(self)->lexeme);
    }
    return type;
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

Node* parser_block(Parser* self, Token* lbrace) {
    Node** nodes = null;
    while (!parser_match_token(self, TOKEN_KIND_RBRACE)) {
        parser_check_eof(self, lbrace);
        Node* node = parser_top_level_node(self);
        buf_push(nodes, node);
    }
    Token* rbrace = parser_previous(self);
    return node_block_new(lbrace, nodes, rbrace);
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
        type = parser_expect_type(self);
    }

    Token* semicolon = parser_expect_semicolon(self);
    return node_variable_decl_new(
            keyword, 
            mut, 
            identifier, 
            type, 
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

    Node* type = null;
    if (parser_current(self)->kind != TOKEN_KIND_LBRACE) {
        type = parser_expect_type(self);
    }

    Node* body = parser_block(self, parser_expect_lbrace(self));
    return node_procedure_decl_new(
            keyword,
            identifier,
            params,
            type,
            body);
}

Node* parser_top_level_node(Parser* self) {
    if (parser_match_keyword(self, PROCEDURE_DECL_KEYWORD)) {
        return parser_procedure_decl(self, parser_previous(self));
    } else if (parser_match_keyword(self, VARIABLE_DECL_KEYWORD)) {
        return parser_variable_decl(self, parser_previous(self));
    } else {
        fatal_error_token(
                parser_current(self),
                "invalid token in top-level");
    }
    assert(0);
    return null;
}

void parser_init(Parser* self, SrcFile* srcfile) {
    self->srcfile = srcfile;
    self->srcfile->nodes = null;
    self->token_idx = 0;
    self->in_function = false;
    self->error = false;
}

void parser_parse(Parser* self) {
    while (parser_current(self)->kind != TOKEN_KIND_EOF) {
        Node* node = parser_top_level_node(self);
        buf_push(self->srcfile->nodes, node);
    }
}
