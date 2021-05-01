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

static void stmt(Resolver* self, Stmt* s);

// returns the previous decl is found
// else null
static Stmt* check_if_symbol_in_sym_tbl(Resolver* self, char* sym) {
	buf_loop(self->current_scope->sym_tbl, st) {
		char* current_sym = null;
		if (self->current_scope->sym_tbl[st]->ty == ST_IMPORTED_NAMESPACE) {
			current_sym = self->current_scope->sym_tbl[st]->imported_namespace.namespace_ident;
		} else {
			current_sym = self->current_scope->sym_tbl[st]->ident->lexeme;
		}

		if (stri(current_sym) == stri(sym)) {
			return self->current_scope->sym_tbl[st];
		}
	}
	return null;
}

static bool add_in_sym_tbl_or_error(Resolver* self, Stmt* s) {
	char* sym = null;
	if (s->ty == ST_IMPORTED_NAMESPACE) {
		sym = s->imported_namespace.namespace_ident;
	} else {
		sym = s->ident->lexeme;
	}

	Stmt* prev_decl = check_if_symbol_in_sym_tbl(self, sym);
	if (prev_decl) {
		error_token(
				self,
				s->ident,
				ERROR_REDECLARATION_OF_SYMBOL,
				sym);
		note_token(
				self,
				prev_decl->ident,
				NOTE_PREVIOUS_SYMBOL_DEFINITION);
		return true;
	} else {
		buf_push(self->current_scope->sym_tbl, s);
		return false;
	}
}

static bool assert_sym_in_sym_tbl_rec_or_error(Resolver* self, Token* ident) {
	Scope* scope = self->current_scope;
	while (scope != null) {
		buf_loop(scope->sym_tbl, s) {
			if (stri(scope->sym_tbl[s]->ident->lexeme) == stri(ident->lexeme)) {
				return true;
			}
		}
		scope = scope->parent;
	}

	error_token(
			self,
			ident,
			ERROR_UNDECLARED_SYMBOL,
			ident->lexeme);
	return false;
}

static void stmt_namespace(Resolver* self, Stmt* s) {
	change_scope(scope);

	bool error = false;
	buf_loop(s->namespace_.stmts, i) {
		bool current_error = add_in_sym_tbl_or_error(self, s->namespace_.stmts[i]);
		if (!error) {
			error = current_error;
		}
	}
	
	if (error) return;

	buf_loop(s->namespace_.stmts, i) {
		stmt(self, s->namespace_.stmts[i]);
	}

	revert_scope(scope);
}	

static void struct_(Resolver* self, DataType* s) {
	change_scope(scope);

	buf_loop(s->struct_.fields, f) {
		add_in_sym_tbl_or_error(self, s->struct_.fields[f]);

		if (s->struct_.fields[f]->variable.variable->dt->ty == DT_NAMED) {
			assert_sym_in_sym_tbl_rec_or_error(self, s->struct_.fields[f]->variable.variable->dt->named.ident);
		} else if (s->struct_.fields[f]->variable.variable->dt->ty == DT_STRUCT) {
			struct_(self, s->struct_.fields[f]->variable.variable->dt);
		}
	}

	revert_scope(scope);
}

static void stmt(Resolver* self, Stmt* s) {
	switch (s->ty) {
		case ST_NAMESPACE:
			stmt_namespace(self, s);
			break;
		case ST_STRUCT:
			struct_(self, s->struct_);
			break;
	}
}

void resolver_resolve(Resolver* self) {
	bool error = false;

	buf_loop(self->srcfile->stmts, s) {
		bool current_error = add_in_sym_tbl_or_error(self, self->srcfile->stmts[s]);
		if (!error) {
			error = current_error;
		}
	}

	buf_loop(self->srcfile->imports, i) {
		bool current_error = add_in_sym_tbl_or_error(self, self->srcfile->imports[i].namespace_);
		if (!error) {
			error = current_error;
		}
	}

	if (error) return;

	buf_loop(self->srcfile->stmts, s) {
		stmt(self, self->srcfile->stmts[s]);
	}
}
