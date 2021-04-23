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

static Token* next(Parser* self) {
	if ((self->token_idx + 1) < buf_len(self->srcfile->tokens)) {
		return self->srcfile->tokens[self->token_idx + 1];
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

#define chkf(stmt) \
	es; stmt; er false;

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

#define expect_lparen(self) \
	chk(expect(self, TT_LPAREN, ERROR_EXPECT_LPAREN))

#define expect_rparen(self) \
	chk(expect(self, TT_RPAREN, ERROR_EXPECT_RBRACE))

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

#define alloc_data_type_named(__name, __static_accessor, __ident) \
	alloc_with_type(__name, DataType); \
	__name->named.static_accessor = __static_accessor; \
	__name->named.ident = __ident;

#define alloc_data_type_struct(__name, __struct_keyword, __ident, __fields) \
	alloc_with_type(__name, DataType); \
	__name->ty = DT_STRUCT; \
	__name->struct_.struct_keyword = __struct_keyword; \
	__name->struct_.ident = __ident; \
	__name->struct_.fields = __fields;

#define alloc_variable(__name, __ident, __dt, __initializer) \
	alloc_with_type(__name, Variable); \
	__name->ident = __ident; \
	__name->dt = __dt; \
	__name->initializer = __initializer;

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
		}

		alloc_variable(field, field_ident, field_dt, null);
		buf_push(fields, field);
	}

	alloc_data_type_struct(struct_dt, struct_keyword, ident, fields);
	return struct_dt;
}

static StaticAccessor parse_static_accessor(Parser* self) {
	StaticAccessor sa;
	sa.accessors = null;
	while (true) {
		if (current(self)->ty == TT_IDENT && next(self)->ty == TT_DOUBLE_COLON) {
			buf_push(sa.accessors, current(self));
			goto_next_token(self);
			goto_next_token(self);
		} else {
			break;
		}
	}
	return sa;
}

static bool match_data_type(Parser* self) {
	if (current(self)->ty == TT_IDENT) {
		StaticAccessor sa = parse_static_accessor(self);

		if (sa.accessors == null) {
			goto_next_token(self);
		} else {
			chkf(expect(self, TT_IDENT, ERROR_EXPECT_IDENT));
		}

		alloc_data_type_named(dt, sa, previous(self));
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

#define alloc_expr_ident(__name, __static_accessor, __ident) \
	alloc_with_type(__name, Expr); \
	__name->ty = ET_IDENT; \
	__name->ident.static_accessor = __static_accessor; \
	__name->ident.ident = __ident;

#define alloc_expr_block(__name, __stmts, __value) \
	alloc_with_type(__name, Expr); \
	__name->ty = ET_BLOCK; \
	__name->block.stmts = __stmts; \
	__name->block.value = __value;

#define alloc_expr_function_call(__name, __left, __args) \
	alloc_with_type(__name, Expr); \
	__name->ty = ET_FUNCTION_CALL; \
	__name->function_call.left = __left; \
	__name->function_call.args = __args;

#define alloc_expr_binary(__name, __ty, __left, __right, __op) \
	alloc_with_type(__name, Expr); \
	__name->ty = __ty; \
	__name->binary.left = __left; \
	__name->binary.right = __right; \
	__name->binary.op = __op;

#define alloc_expr_assign(__name, __left, __right, __op) \
	alloc_with_type(__name, Expr); \
	__name->ty = ET_ASSIGN; \
	__name->assign.left = __left; \
	__name->assign.right = __right; \
	__name->assign.op = __op;

static Stmt* top_level_stmt(Parser* self);
static Expr* expr(Parser* self);

static Expr* expr_atom(Parser* self) {
	if (current(self)->ty == TT_IDENT) {	
		StaticAccessor sa = parse_static_accessor(self);

		if (sa.accessors == null) {
			goto_next_token(self);
		} else {
			expect_ident(self);
		}

		alloc_expr_ident(ident, sa, previous(self));
		return ident;
	} else if (match_token_type(self, TT_LBRACE)) {
		Stmt** stmts = null;
		while (!match_token_type(self, TT_RBRACE)) {
			Stmt* s = top_level_stmt(self);
			if (self->error) return null;
			if (s) buf_push(stmts, s);
		}

		alloc_expr_block(e, stmts, null);
		return e;
	} else if (match_token_type(self, TT_LPAREN)) {
		EXPR(e);
		expect_rparen(self);
		return e;
	} else {
		error_token_with_sync(
				self,
				current(self),
				ERROR_UNEXPECTED_TOKEN
		);
		return null;
	}
}

static Expr* expr_postfix(Parser* self) {
	EXPR_CI(left, expr_atom, self);
	while (match_token_type(self, TT_LPAREN)) {
		Expr** args = null;
		while (!match_token_type(self, TT_RPAREN)) {
			EXPR(arg);
			buf_push(args, arg);
			if (current(self)->ty != TT_RPAREN) {
				expect_comma(self);
			}
		}

		alloc_expr_function_call(e, left, args);
		left = e;
	}
	return left;
}

static Expr* expr_binary_mul_div(Parser* self) {
	EXPR_CI(left, expr_postfix, self);	
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
		EXPR_CI(right, expr_postfix, self);

		alloc_expr_binary(binary, ty, left, right, op);
		left = binary;
	}
	return left;
}

static Expr* expr_binary_add_sub(Parser* self) {
	EXPR_CI(left, expr_binary_mul_div, self);	
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
		EXPR_CI(right, expr_binary_mul_div, self);

		alloc_expr_binary(binary, ty, left, right, op);
		left = binary;
	}
	return left;
}

static Expr* expr_assign(Parser* self) {
	EXPR_CI(left, expr_binary_add_sub, self);
	// TODO: should this be recursive?
	// Or an if statement 
	// Then this would not be possible:
	// `a = b = value;`
	while (match_token_type(self, TT_EQUAL)) {
		Token* op = previous(self);
		EXPR_CI(right, expr_assign, self);

		alloc_expr_assign(assign, left, right, op);
		left = assign;
	}
	return left;
}

static Expr* expr(Parser* self) {
	return expr_assign(self);
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

#define alloc_stmt_namespace(__name, __namespace_keyword, __ident, __stmts) \
	alloc_with_type(__name, Stmt); \
	__name->ty = ST_NAMESPACE; \
	__name->namespace_.namespace_keyword = __namespace_keyword; \
	__name->namespace_.ident = __ident; \
	__name->namespace_.stmts = __stmts;

#define alloc_stmt_struct(__name, __struct_dt) \
	alloc_with_type(__name, Stmt); \
	__name->ty = ST_STRUCT; \
	__name->struct_ = __struct_dt;

#define alloc_stmt_function(__name, __header, __body) \
	alloc_with_type(__name, Stmt); \
	__name->ty = ST_FUNCTION; \
	__name->function.header = __header; \
	__name->function.body = __body;

#define alloc_stmt_function_prototype(__name, __header) \
	alloc_with_type(__name, Stmt); \
	__name->ty = ST_FUNCTION_PROTOTYPE; \
	__name->function_prototype = __header;

#define alloc_function_header(__name, __fn_keyword, __ident, __params, __return_data_type) \
	alloc_with_type(__name, FunctionHeader); \
	__name->fn_keyword = __fn_keyword; \
	__name->ident = __ident; \
	__name->params = __params; \
	__name->return_data_type = __return_data_type;

#define alloc_stmt_variable(__name, __variable, __mut) \
	alloc_with_type(__name, Stmt); \
	__name->ty = ST_VARIABLE; \
	__name->variable.variable = __variable; \
	__name->variable.mut = __mut;

static FunctionHeader* parse_function_header(Parser* self) {
	Token* fn_keyword = previous(self);
	expect_ident(self);
	Token* ident = previous(self);
	expect_lparen(self);

	Variable** params = null;
	while (!match_token_type(self, TT_RPAREN)) {
		expect_ident(self);
		Token* param_ident = previous(self);
		expect_colon(self);

		expect_data_type(self);
		DataType* param_dt = self->matched_dt;
		if (current(self)->ty != TT_RPAREN) {
			expect_comma(self);
		}

		alloc_variable(param, param_ident, param_dt, null);
		buf_push(params, param);
	}

	// TODO: initialize to dt `void`
	DataType* return_data_type = null;
	if (match_token_type(self, TT_COLON)) {
		expect_data_type(self);
		return_data_type = self->matched_dt;
	}

	alloc_function_header(h, fn_keyword, ident, params, return_data_type);
	return h;
}

static Stmt* top_level_stmt(Parser* self) {
	if (match_keyword(self, "struct")) {
		alloc_stmt_struct(s, parse_struct(self, true));
		return s;
	} else if (match_keyword(self, "namespace")) {
		Token* namespace_keyword = previous(self);
		expect_ident(self);
		Token* ident = previous(self);

		Stmt** stmts = null;
		expect_lbrace(self);
		while (!match_token_type(self, TT_RBRACE)) {
			Stmt* s = top_level_stmt(self);
			if (self->error) return null;
			if (s) buf_push(stmts, s);
		}

		alloc_stmt_namespace(s, namespace_keyword, ident, stmts);
		return s;
	} else if (match_keyword(self, "fn")) {
		FunctionHeader* h = null;
		chk(h = parse_function_header(self));

		if (match_token_type(self, TT_SEMICOLON)) {
			alloc_stmt_function_prototype(s, h);
			return s;
		} else {
			expect_lbrace(self);
			goto_previous_token(self);
			EXPR(e);
			alloc_stmt_function(s, h, e);
			return s;
		}

		assert(0);
		return null;
	} else if (match_keyword(self, "let")) {
		bool mut = false;
		if (match_keyword(self, "mut")) {
			mut = true;
		}

		expect_ident(self);
		Token* ident = previous(self);

		DataType* dt = null;
		if (match_token_type(self, TT_COLON)) {
			expect_data_type(self);
			dt = self->matched_dt;
		}

		Expr* initializer = null;
		if (match_token_type(self, TT_EQUAL)) {
			EXPR_ND(initializer, expr, self);
		} else if (!dt) {
			error_token_with_sync(self, current(self), ERROR_EXPECT_INITIALIZER_IF_NO_TYPE_SPECIFIED);
			return null;
		}

		expect_semicolon(self);
		alloc_variable(s_variable, ident, dt, initializer);
		alloc_stmt_variable(s, s_variable, mut);
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
		if (self->error) return;
		if (s) buf_push(self->srcfile->stmts, s); // TODO: is this necessary?
	}
}
