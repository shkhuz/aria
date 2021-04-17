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

void parser_init(Parser* self, SrcFile* srcfile) {
	self->srcfile = srcfile;
	self->token_idx = 0;
}

static void error_token_with_sync(
		Parser* self,
		Token* token,
		u32 code,
		char* fmt,
		...) {

	self->error = true;

	va_list ap;
	va_start(ap, fmt);
	vmsg_user_token(
			MSG_TY_ERR,
			token,
			code,
			fmt,
			ap
	);
	/* sync_to_next_statement(self); */
	// REMOVE THIS
	goto_next_token(self);
	va_end(ap);
}

static Stmt* top_level_stmt(Parser* self) {
	if (match_keyword(self, "fn")) {
		
	} else {
		error_token_with_sync(self, current(self), 10, "invalid top-level token");
	}
	return null;
}

void parser_parse(Parser* self) {
	while (current(self)->ty != TT_EOF) {
		Stmt* s = top_level_stmt(self);
	}
}
