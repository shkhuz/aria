typedef struct {
    SrcFile* srcfile;
    u64 token_idx;
    bool error;
    bool in_procedure;
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
                "while matching `%s`...",
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

#define parser_expect_colon(self) \
    parser_expect( \
            self, \
            TOKEN_KIND_COLON, \
            "expected `:`, got `%s`", \
            parser_current(self)->lexeme)

#define parser_expect_comma(self) \
    parser_expect( \
            self, \
            TOKEN_KIND_COMMA, \
            "expected `,`, got `%s`", \
            parser_current(self)->lexeme)

Node* parser_static_accessor(Parser* self) {
    Token** accessors = null;
    bool from_global_scope = false;
    Token* head = null;

    while (true) {
        if (parser_current(self)->kind == TOKEN_KIND_IDENTIFIER &&
            parser_next(self)->kind == TOKEN_KIND_DOUBLE_COLON) {
            if (!head) head = parser_current(self);
            buf_push(accessors, parser_current(self));
            parser_goto_next_token(self);
            parser_goto_next_token(self);
        } else {
            break;
        }
    }
    return node_static_accessor_new(
            accessors,
            from_global_scope,
            head);
}

Node* parser_type_atom(Parser* self) {
    Node* static_accessor = parser_static_accessor(self);
    Token* identifier = 
        parser_expect_identifier(self, "expected type name, got `%s`");
    TypePrimitiveKind kind = primitive_type_str_to_kind(identifier->lexeme);

    if (kind == TYPE_PRIMITIVE_KIND_NONE) {
        return node_type_custom_new(
                node_symbol_new(static_accessor, identifier));
    } else {
        return node_type_primitive_new(identifier, kind);
    }
    assert(0);
    return null;
}

Node* parser_type_ptr(Parser* self) {
    if (parser_match_token(self, TOKEN_KIND_STAR)) {
        return node_type_ptr_new(
                parser_previous(self), 
                parser_type_ptr(self));
    }
    return parser_type_atom(self);
}

Node* parser_type(Parser* self) {
    return parser_type_ptr(self);
}

Node* parser_symbol(Parser* self) {
    Node* static_accessor = parser_static_accessor(self);
    Token* identifier = parser_expect_identifier(
            self,
            "expected identifier, got `%s`");
    TypePrimitiveKind kind = primitive_type_str_to_kind(identifier->lexeme);

    if (kind == TYPE_PRIMITIVE_KIND_NONE) {
        return node_symbol_new(static_accessor, identifier);
    } else {
        return node_type_primitive_new(identifier, kind);
    }
    assert(0);
    return null;
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
    } else if (parser_match_token(self, TOKEN_KIND_LBRACE)) {
        return parser_block(self, parser_previous(self));
    } else {
        fatal_error_token(
                parser_current(self),
                "`%s` is invalid here",
                parser_current(self)->lexeme);
    }
    return null;
}

Node* parser_expr_unary(Parser* self) {
    if (parser_match_token(self, TOKEN_KIND_MINUS)) {
        Token* op = parser_previous(self);
        Node* right = parser_expr_unary(self);
        return node_expr_unary_new(op, right);
    }
    return parser_expr_atom(self);
}

Node* parser_expr_assign(Parser* self) {
    Node* left = parser_expr_unary(self);
    if (parser_match_token(self, TOKEN_KIND_EQUAL)) {
        Token* op = parser_previous(self);
        Node* right = parser_expr_assign(self);
        return node_expr_assign_new(
                op,
                left,
                right);
    }
    return left;
}

Node* parser_expr(Parser* self) {
    return parser_expr_assign(self);
}

Node* parser_expr_stmt(Parser* self) {
    Node* node = parser_expr(self);
    Token* tail = null;
    if (!node) return null;

    if (node->kind != NODE_KIND_BLOCK) {
        tail = parser_expect_semicolon(self);
    } else {
        tail = node->tail;
    }
    return node_expr_stmt_new(node, tail);
}

Node* parser_variable_decl(Parser* self, Token* keyword, bool constant) {
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
            constant, 
            identifier, 
            type, 
            initializer,
            semicolon,
            self->in_procedure);
}

Node* parser_procedure_decl(Parser* self, Token* keyword) {
    self->in_procedure = true;
    Token* identifier = 
        parser_expect_identifier(self, "expected procedure name, got `%s`");

    Node** params = null;
    Token* lparen = parser_expect_lparen(self);
    while (!parser_match_token(self, TOKEN_KIND_RPAREN)) {
        parser_check_eof(self, lparen);

        Token* param_identifier = 
            parser_expect_identifier(self, "expected parameter name");
        parser_expect_colon(self);
        Node* param_type = parser_type(self);
        buf_push(params, node_param_new(param_identifier, param_type));

        if (parser_current(self)->kind != TOKEN_KIND_RPAREN) {
            parser_expect_comma(self);
        }
    }

    Node* type = primitive_type_placeholders.void_;
    if (parser_current(self)->kind != TOKEN_KIND_LBRACE) {
        type = parser_type(self);
    } 

    Node* body = parser_block(self, parser_expect_lbrace(self));

    self->in_procedure = false;
    return node_procedure_decl_new(
            keyword,
            identifier,
            params,
            type,
            body);
}

Node* parser_top_level_node(Parser* self, bool error_on_no_match) {
    if (parser_match_keyword(self, "import")) {
        Token* import_keyword = parser_previous(self);
        if (!parser_match_token(self, TOKEN_KIND_IDENTIFIER)) {
            fatal_error_token(
                    parser_current(self),
                    "expected module name");
        }
        Token* import_module_token = parser_previous(self);
        // relative to this file
        char* import_filename = aria_strapp(
                stri(import_module_token->lexeme),
                ".ar");
        parser_expect_semicolon(self);

        char* current_file_path = stri(self->srcfile->contents->fpath);
        char* last_slash_ptr = strrchr(current_file_path, '/');
        uint last_slash_idx = 0;
        if (last_slash_ptr) {
            last_slash_idx = last_slash_ptr - current_file_path;
        }

        char* current_dir_path = aria_strsub(
                current_file_path,
                0,
                (last_slash_idx == 0 ? 0 : last_slash_idx + 1));
        // relative to compiler
        char* import_file_path = aria_strapp(
                (current_dir_path == null ? "" : current_dir_path),
                import_filename);
        return node_implicit_module_new(
                import_keyword,
                import_module_token,
                import_filename,
                import_file_path);
    } else if (parser_match_keyword(self, PROCEDURE_DECL_KEYWORD)) {
        return parser_procedure_decl(self, parser_previous(self));
    } else if (parser_match_keyword(self, VARIABLE_DECL_KEYWORD)) {
        return parser_variable_decl(self, parser_previous(self), false);
    } else if (parser_match_keyword(self, CONSTANT_DECL_KEYWORD)) {
        return parser_variable_decl(self, parser_previous(self), true);
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
    if (parser_match_keyword(self, VARIABLE_DECL_KEYWORD)) {
        return parser_variable_decl(self, parser_previous(self), false);
    } else if (parser_match_keyword(self, CONSTANT_DECL_KEYWORD)) {
        return parser_variable_decl(self, parser_previous(self), true);
    }
    return parser_expr_stmt(self);
}

void parser_init(Parser* self, SrcFile* srcfile) {
    self->srcfile = srcfile;
    self->srcfile->nodes = null;
    self->token_idx = 0;
    self->error = false;
    self->in_procedure = false;
}

void parser_parse(Parser* self) {
    while (parser_current(self)->kind != TOKEN_KIND_EOF) {
        Node* node = parser_top_level_node(self, true);
        buf_push(self->srcfile->nodes, node);
    }
}
