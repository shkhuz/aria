#include <aria_core.h>
#include <aria.h>

static bool match_data_type(Parser* self);

static Token* current(Parser* self) {
	if (self->token_idx < buf_len(self->srcfile->tokens)) {
		return self->srcfile->tokens[self->token_idx];
	}
	assert(0);
	return null;
}

static Token* previous(Parser* self) {
	if (self->token_idx > 0) {
		return self->srcfile->tokens[self->token_idx - 1];
	}
	assert(0);
	return null;
}

static void goto_next_token(Parser* self) {
	if (self->token_idx < buf_len(self->srcfile->tokens)) {
		self->token_idx++;
	}
}

static void goto_previous_token(Parser* self) {
	if (self->token_idx > 0) {
		self->token_idx--;
	}
}

static bool match_token_type(Parser* self, TokenType ty) {
	if (current(self)->ty == ty) {
		goto_next_token(self);
		return true;
	}
	return false;
}

#define match_ident(self) \
	match_token_type(self, TT_IDENT)

static bool match_keyword(Parser* self, char* keyword) {
	if (current(self)->ty == TT_KEYWORD) {
		if (stri(current(self)->lexeme) == stri(keyword)) {
			goto_next_token(self);
			return true;
		}
	}
	return false;
}

static void error_token_with_sync(
		Parser* self,
		Token* token,
		u32 code,
		char* fmt,
		...) {

	self->error = true;
	self->error_count++;

	va_list ap;
	va_start(ap, fmt);
	vmsg_user_token(
			MSG_TY_ERR,
			token,
			code,
			fmt,
			ap
	);
	va_end(ap);
}

static void expect(Parser* self, TokenType ty, u32 code, char* fmt, ...) {
	if (!match_token_type(self, ty)) {
		va_list ap;
		va_start(ap, fmt);
		error_token_with_sync(self, current(self), code, fmt, ap);
		va_end(ap);
	}
}

#define es \
	u64 COMBINE(_error_store,__LINE__) = self->error_count

#define er \
	if (self->error_count > COMBINE(_error_store,__LINE__)) return

#define ec \
	if (self->error_count > COMBINE(_error_store,__LINE__)) continue

#define ern \
	er null

#define chk(stmt) \
	es; stmt; ern;

// void
#define chkv(stmt) \
	es; stmt; er;

// loop
#define chklp(stmt) \
	es; stmt; ec;

/* custom initialization */
#define EXPR_CI(name, init_func, ...) \
	Expr* name = null; \
	{ \
		es; \
		name = init_func(__VA_ARGS__); \
		ern; \
	}

/* custom initialization; continue if error */
#define EXPR_CI_LP(name, init_func, ...) \
	Expr* name = null; \
	{ \
		es; \
		name = init_func(__VA_ARGS__); \
		ec; \
	}

/* no declaration */
#define EXPR_ND(name, init_func, ...) \
	{ \
		es; \
		name = init_func(__VA_ARGS__); \
		ern; \
	}

#define EXPR(name) \
	EXPR_CI(name, expr, self)

#define expect_semicolon(self) \
	chk(expect(self, TT_SEMICOLON, ERROR_EXPECT_SEMICOLON))

#define expect_colon(self) \
	chk(expect(self, TT_COLON, ERROR_EXPECT_COLON))

#define expect_comma(self) \
	chk(expect(self, TT_COMMA, ERROR_EXPECT_COMMA))

#define expect_ident(self) \
	chk(expect(self, TT_IDENT, ERROR_EXPECT_IDENT))

#define expect_lbrace(self) \
	chk(expect(self, TT_LBRACE, ERROR_EXPECT_LBRACE))

#define expect_rbrace(self) \
	chk(expect(self, TT_RBRACE, ERROR_EXPECT_RBRACE))

#define chkv_match_data_type(self, __matched) \
	bool __matched = false; \
	chkv(__matched = match_data_type(self));

#define chk_match_data_type(self, __matched) \
	bool __matched = false; \
	chk(__matched = match_data_type(self));

static void __expect_data_type(Parser* self) {
	chkv_match_data_type(self, matched);
	if (!matched) {
		error_token_with_sync(self, current(self), ERROR_EXPECT_DATA_TYPE);
	}
}

#define expect_data_type(self) \
	chk(__expect_data_type(self))

#define alloc_data_type_named(__name, __ident) \
	alloc_with_type(__name, DataType); \
	__name->named.ident = __ident;

#define alloc_data_type_struct(__name, __struct_keyword, __ident, __fields) \
	alloc_with_type(__name, DataType); \
	__name->ty = DT_STRUCT; \
	__name->struct_.struct_keyword = __struct_keyword; \
	__name->struct_.ident = __ident; \
	__name->struct_.fields = __fields;

#define alloc_variable(__name, __ident, __dt) \
	alloc_with_type(__name, Variable); \
	__name->ident = __ident; \
	__name->dt = __dt;

static DataType* parse_struct(Parser* self, bool is_stmt) {
	Token* struct_keyword = previous(self);

	Token* ident = null;
	if (is_stmt) {
		expect_ident(self);
		ident = previous(self);
	} else if (match_ident(self)) {
		ident = previous(self);
	}
	expect_lbrace(self);

	Variable** fields = null;
	while (!match_token_type(self, TT_RBRACE)) {
		expect_ident(self);
		Token* field_ident = previous(self);
		expect_colon(self);

		expect_data_type(self);
		DataType* field_dt = self->matched_dt;
		if (current(self)->ty != TT_RBRACE) {
			expect_comma(self);
		} //else match(self, T_COMMA);

		alloc_variable(field, field_ident, field_dt);
		buf_push(fields, field);
	}

	alloc_data_type_struct(struct_dt, struct_keyword, ident, fields);
	return struct_dt;
}

static bool match_data_type(Parser* self) {
	if (match_ident(self)) {
		alloc_data_type_named(dt, previous(self));
		self->matched_dt = dt;
		return true;
	} else if (match_keyword(self, "struct")) {
		self->matched_dt = parse_struct(self, false);
		if (self->matched_dt) {
			return true;
		}
	}
	return false;
}

#define alloc_expr_ident(__name, __ident) \
	alloc_with_type(__name, Expr); \
	__name->ty = ET_IDENT; \
	__name->ident = __ident;


#define alloc_expr_binary(__name, __ty, __left, __right, __op) \
	alloc_with_type(__name, Expr); \
	__name->ty = __ty; \
	__name->binary.left = __left; \
	__name->binary.right = __right; \
	__name->binary.op = __op;

static Expr* expr_precedence_1(Parser* self) {
	if (match_ident(self)) {	
		alloc_expr_ident(ident, previous(self));
		return ident;
	} else {
		error_token_with_sync(
				self,
				current(self),
				ERROR_UNEXPECTED_TOKEN
		);
		return null;
	}
}

static Expr* expr_precedence_2(Parser* self) {
	EXPR_CI(left, expr_precedence_1, self);	
	while (match_token_type(self, TT_STAR) || match_token_type(self, TT_FSLASH)) {
		Token* op = previous(self);
		ExprType ty;
		switch (op->ty) {
			case TT_STAR:
				ty = ET_BINARY_MULTIPLY;
				break;
			case TT_FSLASH: 
				ty = ET_BINARY_DIVIDE;
				break;
			default:
				break;
		}
		EXPR_CI(right, expr_precedence_1, self);

		alloc_expr_binary(binary, ty, left, right, op);
		left = binary;
	}
	return left;
}

static Expr* expr_precedence_3(Parser* self) {
	EXPR_CI(left, expr_precedence_2, self);	
	while (match_token_type(self, TT_PLUS) || match_token_type(self, TT_MINUS)) {
		Token* op = previous(self);
		ExprType ty;
		switch (op->ty) {
			case TT_PLUS:
				ty = ET_BINARY_ADD;
				break;
			case TT_MINUS: 
				ty = ET_BINARY_SUBTRACT;
				break;
			default:
				break;
		}
		EXPR_CI(right, expr_precedence_2, self);

		alloc_expr_binary(binary, ty, left, right, op);
		left = binary;
	}
	return left;
}

static Expr* expr(Parser* self) {
	return expr_precedence_3(self);
}	

#define alloc_stmt_expr(__name, __expr) \
	alloc_with_type(__name, Stmt); \
	__name->ty = ST_EXPR; \
	__name->expr = __expr;

static Stmt* stmt_expr(Parser* self) {
	EXPR(e);
	expect_semicolon(self);
	alloc_stmt_expr(s, e);
	return s;
}

#define alloc_stmt_struct(__name, __struct_dt) \
	alloc_with_type(__name, Stmt); \
	__name->ty = ST_STRUCT; \
	__name->struct_ = __struct_dt;

#define alloc_stmt_variable(__name, __ident, __dt, __initializer) \
	alloc_with_type(__name, Stmt); \
	__name->ty = ST_VARIABLE; \
	__name->variable.ident = __ident; \
	__name->variable.dt = __dt; \
	__name->variable.initializer = __initializer;

static Stmt* top_level_stmt(Parser* self) {
	if (match_keyword(self, "fn")) {
		return null;
	} else if (match_keyword(self, "struct")) {
		alloc_stmt_struct(s, parse_struct(self, true));
		return s;
	} else if (current(self)->ty == TT_IDENT && self->srcfile->tokens[self->token_idx + 1]->ty == TT_COLON) {
		Token* ident = current(self);
		goto_next_token(self);
		expect_colon(self);

		chk_match_data_type(self, matched);
		DataType* dt = (matched ? self->matched_dt : null);

		Expr* initializer = null;
		if (match_token_type(self, TT_EQUAL)) {
			EXPR_ND(initializer, expr, self);
		} else if (!matched) {
			error_token_with_sync(self, current(self), ERROR_EXPECT_INITIALIZER_IF_NO_TYPE_SPECIFIED);
			return null;
		}

		expect_semicolon(self);
		alloc_stmt_variable(s, ident, dt, initializer);
		return s;
	} else {
		/* error_token_with_sync(self, current(self), 10, "invalid top-level token"); */
		return stmt_expr(self);
	}
	assert(0);
	return null;
}

void parser_init(Parser* self, SrcFile* srcfile) {
	self->srcfile = srcfile;
	self->token_idx = 0;
	self->matched_dt = null;
	self->srcfile->stmts = null;
	self->error = false;
	self->error_count = 0;
}

void parser_parse(Parser* self) {
	while (current(self)->ty != TT_EOF) {
		Stmt* s = top_level_stmt(self);
		if (s) buf_push(self->srcfile->stmts, s);
		if (self->error) return;
	}
}
