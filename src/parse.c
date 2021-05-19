typedef struct {
	SrcFile* srcfile;
	u64 token_idx;
	bool in_function;
	bool error;
} Parser;

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

bool parser_match_token(Parser* self, TokenKind kind) {
	if (parser_current(self)->kind == kind) {
		parser_goto_next_token(self);
		return true;
	}
	return false;
}

bool parser_match_keyword(Parser* self, char* keyword) {
	if (parser_current(self)->kind == TK_KEYWORD) {
		if (stri(parser_current(self)->lexeme) == stri(keyword)) {
			parser_goto_next_token(self);
			return true;
		}
	}
	return false;
}

Token* expect(Parser* self, TokenKind kind, char* fmt, ...) {
	if (!parser_match_token(self, kind)) {
		va_list ap;
		va_start(ap, fmt);
		fatal_verror_token(parser_current(self), fmt, ap);
		va_end(ap);
		return null;
	}
	return parser_previous(self);
}

#define expect_identifier(self, fmt) \
	expect(self, TK_IDENTIFIER, fmt, parser_current(self)->lexeme)

#define expect_semicolon(self, fmt) \
	expect(self, TK_SEMICOLON, fmt, parser_current(self)->lexeme)

Node* parser_variable_decl(Parser* self, Token* keyword) {
	bool mut = false;
	if (parser_match_keyword(self, "mut")) {
		mut = true;
	}
	
	Token* identifier = 
		expect_identifier(self, "expected variable name, got `%s`");

	Token* semicolon = expect_semicolon(self, "expected `;`, got `%s`");
	return node_variable_new(keyword, semicolon, mut, identifier);
}

Node* parser_top_level_node(Parser* self) {
	if (parser_match_keyword(self, "let")) {
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
	self->token_idx = 0;
	self->in_function = false;
	self->srcfile->nodes = null;
	self->error = false;
}

void parser_parse(Parser* self) {
	while (parser_current(self)->kind != TK_EOF) {
		Node* node = parser_top_level_node(self);
		buf_push(self->srcfile->nodes, node);
	}
}
