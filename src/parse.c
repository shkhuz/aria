#include <aria_core.h>
#include <aria.h>

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

static bool match_ty(Parser* self, TokenType ty) {
	if (current(self)->ty == ty) {
		goto_next_token(self);
		return true;
	}
	return false;
}

static bool match_keyword(Parser* self, char* keyword) {
	if (current(self)->ty == TT_KEYWORD) {
		for (uint i = 0; i < stack_arr_len(keywords); i++) {
			if (stri(keywords[i]) == stri(keyword)) {
				goto_next_token(self);
				return true;
			}
		}
	}
	return false;
}

///// ERROR HANDLING /////
static void sync_to_next_stmt(Parser* self) {
	while (!match_ty(self, TT_SEMICOLON) && current(self)->ty != TT_LBRACE) {
		if (current(self)->ty == TT_EOF) return;
		goto_next_token(self);
	}
	// TODO: implement brace skipping
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
	sync_to_next_stmt(self);
	va_end(ap);
}

static void expect(Parser* self, TokenType ty, u32 code, char* fmt, ...) {
    if (!match_ty(self, ty)) {
        va_list ap;
        va_start(ap, fmt);
        error_token_with_sync(self, current(self), code, fmt, ap);
        va_end(ap);
    }
}

#define expect_semicolon(self) \
	chk(expect(self, TT_SEMICOLON, ERROR_EXPECT_SEMICOLON))

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

#define chkv(stmt) \
    es; stmt; er;

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
	if (match_ty(self, TT_IDENT)) {	
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
	while (match_ty(self, TT_STAR) || match_ty(self, TT_FSLASH)) {
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
	while (match_ty(self, TT_PLUS) || match_ty(self, TT_MINUS)) {
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

static Stmt* stmt_expr(Parser* self) {
	EXPR(e);
	expect_semicolon(self);

	alloc_with_type(stmt, Stmt);
	stmt->ty = ST_EXPR;
	stmt->expr = e;
	return stmt;
}

static Stmt* top_level_stmt(Parser* self) {
	if (match_keyword(self, "fn")) {
		
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
	self->srcfile->stmts = null;
	self->error = false;
	self->error_count = 0;
}

void parser_parse(Parser* self) {
	while (current(self)->ty != TT_EOF) {
		Stmt* s = top_level_stmt(self);
		if (s) buf_push(self->srcfile->stmts, s);
	}
}
