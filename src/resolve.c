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
static bool struct_(Resolver* self, DataType* s);

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
				return false;
			}
		}
		scope = scope->parent;
	}

	error_token(
			self,
			ident,
			ERROR_UNDECLARED_SYMBOL,
			ident->lexeme);
	return true;
}

static void expr_block(Resolver* self, Expr* e) {
	change_scope(scope);

	buf_loop(e->block.stmts, s) {
		stmt(self, e->block.stmts[s]);
	}

	revert_scope(scope);
}

static void expr(Resolver* self, Expr* e) {
	switch (e->ty) {
		case ET_BLOCK:
			expr_block(self, e);
			break;
	}
}

static void stmt_namespace(Resolver* self, Stmt* s) {
	change_scope(scope);

	bool error = false;
	buf_loop(s->namespace_.stmts, i) {
		bool current_error = add_in_sym_tbl_or_error(self, s->namespace_.stmts[i]);
		if (!error) error = current_error;
	}
	
	if (error) return;

	buf_loop(s->namespace_.stmts, i) {
		stmt(self, s->namespace_.stmts[i]);
	}

	revert_scope(scope);
}	

static bool data_type(Resolver* self, DataType* dt) {
	if (dt->ty == DT_NAMED) {
		return assert_sym_in_sym_tbl_rec_or_error(self, dt->named.ident);
	} else if (dt->ty == DT_STRUCT) {
		return struct_(self, dt);
	}
	assert(0);
	return false;
}

static bool struct_(Resolver* self, DataType* s) {
	change_scope(scope);

	bool error = false;
	buf_loop(s->struct_.fields, f) {
		bool current_error = add_in_sym_tbl_or_error(self, s->struct_.fields[f]);
		if (!error) error = current_error;

		current_error = data_type(self, s->struct_.fields[f]->variable.variable->dt);
		if (!error) error = current_error;
	}

	if (error) return true;

	revert_scope(scope);
	return false;
}

static void stmt_function(Resolver* self, Stmt* s) {
	change_scope(scope);

	bool error = false;
	buf_loop(s->function.header->params, p) {
		bool current_error = add_in_sym_tbl_or_error(self, s->function.header->params[p]);
		if (!error) error = current_error;

		current_error = data_type(self, s->function.header->params[p]->variable.variable->dt);
		if (!error) error = current_error;
	}

	if (s->function.header->return_data_type) {
		bool current_error = data_type(self, s->function.header->return_data_type);
		if (!error) error = current_error;
	}


	if (error) return;

	buf_loop(s->function.body->block.stmts, c) {
		stmt(self, s->function.body->block.stmts[c]);
	}

	revert_scope(scope);
}

static void stmt_variable(Resolver* self, Stmt* s) {
	add_in_sym_tbl_or_error(self, s);
	if (s->variable.variable->dt) {
		data_type(self, s->variable.variable->dt);
	}
	if (s->variable.variable->initializer) {
		expr(self, s->variable.variable->initializer);
	}
}

static void stmt_expr(Resolver* self, Stmt* s) {
	expr(self, s->expr);
}

static void stmt(Resolver* self, Stmt* s) {
	switch (s->ty) {
		case ST_NAMESPACE:
			stmt_namespace(self, s);
			break;
		case ST_STRUCT:
			struct_(self, s->struct_);
			break;
		case ST_FUNCTION:
			stmt_function(self, s); 
			break;
		case ST_VARIABLE:
			stmt_variable(self, s); 
			break;
		case ST_EXPR: 
			stmt_expr(self, s);
			break;
	}
}

void resolver_resolve(Resolver* self) {
	bool error = false;

	buf_loop(self->srcfile->stmts, s) {
		bool current_error = add_in_sym_tbl_or_error(self, self->srcfile->stmts[s]);
		if (!error) error = current_error;
	}

	buf_loop(self->srcfile->imports, i) {
		bool current_error = add_in_sym_tbl_or_error(self, self->srcfile->imports[i].namespace_);
		if (!error) error = current_error;
	}

	if (error) return;

	buf_loop(self->srcfile->stmts, s) {
		stmt(self, self->srcfile->stmts[s]);
	}
}
