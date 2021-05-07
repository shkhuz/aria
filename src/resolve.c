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
	self->dont_create_block_scope = false;
	self->error = false;
} 

static void stmt(Resolver* self, Stmt* s);
static bool struct_(Resolver* self, DataType* s);
static void expr(Resolver* self, Expr* e);

char* one_word_stmt_ty(Stmt* s) {
	if (s->ty == ST_NAMESPACE || s->ty == ST_IMPORTED_NAMESPACE) return "namespace";
	else if (s->ty == ST_STRUCT) return "type";
	else if (s->ty == ST_FUNCTION) return "function";
	assert(0);
}

static char* stmt_get_ident(Stmt* s) {
	if (s->ty == ST_IMPORTED_NAMESPACE) return s->imported_namespace.ident;
	else return s->ident->lexeme;
}

// returns the previous decl is found
// else null
static Stmt* check_if_symbol_in_sym_tbl(Resolver* self, char* sym) {
	buf_loop(self->current_scope->sym_tbl, st) {
		if (stri(stmt_get_ident(self->current_scope->sym_tbl[st])) == stri(sym)) {
			return self->current_scope->sym_tbl[st];
		}
	}
	return null;
}

static bool add_in_sym_tbl_or_error(Resolver* self, Stmt* s) {
	Stmt* prev_decl = check_if_symbol_in_sym_tbl(self, stmt_get_ident(s));
	if (prev_decl) {
		error_token(
				self,
				s->ident,
				ERROR_REDECLARATION_OF_SYMBOL,
				stmt_get_ident(s));
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

static Stmt* assert_sym_in_sym_tbl_rec_or_error(Resolver* self, Token* ident, bool global_scope) {
	Scope* scope;
	if (global_scope) scope = self->global_scope;
	else scope = self->current_scope;

	while (scope != null) {
		buf_loop(scope->sym_tbl, s) {
			if (stri(stmt_get_ident(scope->sym_tbl[s])) == stri(ident->lexeme)) {
				return scope->sym_tbl[s];
			}
		}
		scope = scope->parent;
	}

	error_token(
			self,
			ident,
			ERROR_UNDECLARED_SYMBOL,
			ident->lexeme);
	return null;
}

// TODO: refactor
static Stmt** get_static_accessor_namespace(Resolver* self, StaticAccessor sa) {
	Token** accessors = sa.accessors;
	Scope* scope = self->current_scope;
	Stmt** namespace_body = null;

	if (sa.from_global_scope) {
		namespace_body = self->srcfile->stmts;
	} else {
		bool first_accessor_found = false;
		while (scope != null) {
			buf_loop(scope->sym_tbl, s) {
				if (scope->sym_tbl[s]->ty == ST_NAMESPACE && stri(scope->sym_tbl[s]->ident->lexeme) == stri(accessors[0]->lexeme)) {
					namespace_body = scope->sym_tbl[s]->namespace_.stmts;
					first_accessor_found = true;	// TODO: refactor these two stmts
					break;
				}
				else if (scope->sym_tbl[s]->ty == ST_IMPORTED_NAMESPACE && stri(scope->sym_tbl[s]->imported_namespace.ident) == stri(accessors[0]->lexeme)) {
					namespace_body = scope->sym_tbl[s]->imported_namespace.srcfile->stmts;
					first_accessor_found = true;
					break;
				}
			}

			if (first_accessor_found) break;
			scope = scope->parent;
		}

		if (namespace_body == null) {
			error_token(
					self, 
					accessors[0],
					ERROR_NAMESPACE_NOT_FOUND,
					accessors[0]->lexeme);
			return null;
		}
	}

	// TODO: u64??
	u64 start_idx = (sa.from_global_scope ? 0 : 1);
	for (u64 a = start_idx; a < buf_len(accessors); a++) {
		bool found = false;
		buf_loop(namespace_body, s) {
			if (namespace_body[s]->ty == ST_NAMESPACE && 
				stri(namespace_body[s]->ident->lexeme) == stri(accessors[a]->lexeme)) {
				found = true;	// TODO: refactor these three stmts
				namespace_body = namespace_body[s]->namespace_.stmts;
				break;
			} else if (namespace_body[s]->ty == ST_IMPORTED_NAMESPACE &&
					   stri(namespace_body[s]->imported_namespace.ident) == stri(accessors[a]->lexeme)) {
				found = true;
				namespace_body = namespace_body[s]->imported_namespace.srcfile->stmts;
				break;
			}
		}

		if (!found) {
			error_token(
					self, 
					accessors[a],
					ERROR_NAMESPACE_NOT_FOUND,
					accessors[a]->lexeme);
			return null;
		}
	}
	return namespace_body;
}

static Stmt* assert_sym_in_stmt_buffer(Resolver* self, Token* sym, Stmt** stmts) {
	buf_loop(stmts, s) {
		if (stri(stmt_get_ident(stmts[s])) == stri(sym->lexeme)) {
			return stmts[s];
		}
	}

	error_token(
			self, 
			sym,
			ERROR_UNDECLARED_SYMBOL,
			sym->lexeme);
	return null;
}

static void expr_block(Resolver* self, Expr* e) {
	Scope* scope = null;
	if (!self->dont_create_block_scope) {
		scope = scope_new(self->current_scope, null);
		self->current_scope = scope;
	}

	buf_loop(e->block.stmts, s) {
		if (e->block.stmts[s]->ty == ST_STRUCT || e->block.stmts[s]->ty == ST_FUNCTION) {
			add_in_sym_tbl_or_error(self, e->block.stmts[s]);
		}
	}

	buf_loop(e->block.stmts, s) {
		stmt(self, e->block.stmts[s]);
	}

	if (!self->dont_create_block_scope) {
		self->current_scope = scope->parent;
	}
}

static Stmt* assert_static_accessor_ident_in_scope(Resolver* self, StaticAccessor sa, Token* ident) {
	if (sa.accessors) {
		Stmt** namespace_body = get_static_accessor_namespace(self, sa);
		if (namespace_body) {
			return assert_sym_in_stmt_buffer(self, ident, namespace_body);
		} else {
			return null;
		}
	} else {
		return assert_sym_in_sym_tbl_rec_or_error(self, ident, sa.from_global_scope);
	}
	assert(0);
	return null;
}

static void expr_ident(Resolver* self, Expr* e) {
	e->ident.ref = assert_static_accessor_ident_in_scope(self, e->ident.static_accessor, e->ident.ident);
}

static void expr_function_call(Resolver* self, Expr* e) {
	expr(self, e->function_call.left);
	buf_loop(e->function_call.args, a) {
		expr(self, e->function_call.args[a]);
	}
}

static void expr_assign(Resolver* self, Expr* e) {
	if (e->assign.left->ty != ET_IDENT) { // TODO: add pointer ref/deref, ...
		error_token(
				self,
				e->assign.left->head,
				ERROR_INVALID_LVALUE);
		return;
	}

	expr(self, e->assign.left);
   	expr(self, e->assign.right);
	if (!e->assign.left->ident.ref) return;

	if (e->assign.left->ident.ref->ty != ST_VARIABLE) {
		error_token(
				self,
				e->assign.left->head,
				ERROR_CANNOT_ASSIGN_TO,
				one_word_stmt_ty(e->assign.left->ident.ref));
		// TODO add note defined here (not done because of pointer ref/deref refactoring
		return;
	}

	if (!e->assign.left->ident.ref->variable.is_mut) {
		error_token(
				self, 
				e->assign.left->head,
				ERROR_CANNOT_ASSIGN_TO_IMMUTABLE);
		// TODO add note defined here
	}
}

static void expr(Resolver* self, Expr* e) {
	switch (e->ty) {
		case ET_BLOCK:
			expr_block(self, e);
			break;
		case ET_IDENT:
			expr_ident(self, e);
			break;
		case ET_FUNCTION_CALL:
			expr_function_call(self, e);
			break;
		case ET_ASSIGN:
			expr_assign(self, e);
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
		dt->named.ref = assert_static_accessor_ident_in_scope(self, dt->named.static_accessor, dt->named.ident);
		return (dt->named.ref ? false : true);
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

	self->dont_create_block_scope = true;
	expr(self, s->function.body);
	self->dont_create_block_scope = false;

	revert_scope(scope);
}

static void stmt_variable(Resolver* self, Stmt* s) {
	if (s->in_function) {
		add_in_sym_tbl_or_error(self, s);
	}
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

	if (error) return;

	buf_loop(self->srcfile->stmts, s) {
		stmt(self, self->srcfile->stmts[s]);
	}
}
