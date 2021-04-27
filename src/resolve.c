#include <aria_core.h>
#include <aria.h>

static Scope* scope_new(Scope* parent, Stmt** sym_tbl) {
	alloc_with_type(scope, Scope);
	scope->parent = parent;
	scope->sym_tbl = sym_tbl;
	return scope;
}

#define change_scope(__scope) \
	Scope* __scope = scope_new(self->current_scope, null); \
	self->current_scope = __scope;

#define revert_scope(__scope) \
	self->current_scope = __scope->parent;

static void error_token(Resolver* self, Token* token, u32 code, char* fmt, ...) {
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
	va_end(ap);
}

#define note_token(self, token, fmt, ...) \
	msg_user_token( \
			MSG_TY_NOTE, \
			token, \
			0, \
			fmt, \
			##__VA_ARGS__);

void resolver_init(Resolver* self, SrcFile* srcfile) {
	self->srcfile = srcfile;
	self->global_scope = scope_new(null, null);
	self->current_scope = self->global_scope;
	self->error = false;
} 

static Stmt* check_if_symbol_in_sym_tbl(Resolver* self, Stmt* s) {
	buf_loop(self->current_scope->sym_tbl, st) {
		if (token_lexeme_eql(
					self->current_scope->sym_tbl[st]->ident, 
					s->ident)) {
			return self->current_scope->sym_tbl[st];
		}
	}
	return null;
}

void resolver_resolve(Resolver* self) {
	Stmt** stmts = self->srcfile->stmts;
	buf_loop(stmts, s) {
		Stmt* prev_decl = check_if_symbol_in_sym_tbl(self, stmts[s]);
		if (prev_decl) {
			error_token(
					self,
					stmts[s]->ident,
					ERROR_REDECLARATION_OF_SYMBOL,
					stmts[s]->ident->lexeme);
			note_token(
					self,
					prev_decl->ident,
					NOTE_PREVIOUS_SYMBOL_DEFINITION);

		} else {
			buf_push(self->current_scope->sym_tbl, stmts[s]);
		}
	}
}
