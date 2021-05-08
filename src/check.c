#include <aria_core.h>
#include <aria.h>

#include <thirdparty/Defer/defer.h>

static void stmt(Checker* self, Stmt* s);
static DataType* expr(Checker* self, Expr* e);

static void set_error_flags(Checker* self) {
	self->error = true;
	self->error_count++;
}

static void error_token(Checker* self, Token* token, u32 code, char* fmt, ...) {
	set_error_flags(self);

	va_list ap;
	va_start(ap, fmt);
	vmsg_user_token(
			MSG_TY_ERR,
			token,
			code,
			fmt,
			ap);
	va_end(ap);
}

static void error_expr(Checker* self, Expr* expr, u32 code, char* fmt, ...) {
	set_error_flags(self);

	va_list ap;
	va_start(ap, fmt);
	vmsg_user_expr(
			MSG_TY_ERR,
			expr, 
			code,
			fmt, 
			ap);
	va_end(ap);
}

#define note_token(self, token, fmt, ...) \
	msg_user_token( \
			MSG_TY_NOTE, \
			token, \
			0, \
			fmt, \
			##__VA_ARGS__);

#define write_static_accessor_to_buf(b, sa) \
	if (sa.from_global_scope) { buf_write(b, "::"); } \
	buf_loop(sa.accessors, COMBINE(i, __LINE__)) { \
		buf_write(b, sa.accessors[COMBINE(i, __LINE__)]->lexeme); \
		buf_write(b, "::"); \
	}

static char* data_type_to_string(DataType* dt) {
	char* buf = null;
	buf_clear(buf);

	if (dt->ty == DT_NAMED) {
		buf_loop(dt->named.pointers, p) {
			buf_push(buf, '*');
			if (dt->named.pointers[p].is_const) {
				buf_write(buf, "const ");
			}
		}

		write_static_accessor_to_buf(buf, dt->named.ident->ident.static_accessor);
		buf_write(buf, dt->named.ident->ident.ident->lexeme);
	} else if (dt->ty == DT_STRUCT) {
		// TODO: do we also want to output fields?
		buf_write(buf, "<anonymous struct>");
	}

	buf_push(buf, '\0');
	return buf;
}

static Token* data_type_get_token(DataType* dt) {
	if (dt->ty == DT_NAMED) return dt->named.ident->ident.ident;
	else if (dt->ty == DT_STRUCT) return dt->struct_.struct_keyword;
	assert(0);
}

static bool can_data_types_coerce(DataType* a, DataType* b) {
	if (!a || !b) return false;
	if (a->ty != b->ty) return false;

	if (a->ty == DT_NAMED) {
		if (buf_len(a->named.pointers) == buf_len(b->named.pointers) && // TODO: const check
			a->named.ref == b->named.ref) {
			return true;
		}
		return false;
	} else if (a->ty == DT_STRUCT) {
		// TODO
	}
	assert(0);
	return false;
}

static char* expr_ident_get_full_lexeme(Expr* e) {
	char* buf = null;
	write_static_accessor_to_buf(buf, e->ident.static_accessor);
	buf_write(buf, e->ident.ident->lexeme);
	return buf;
}

static bool data_type(Checker* self, DataType* dt, bool print_err) {Deferral
	if (dt->ty == DT_NAMED) {
		if (dt->named.ref->ty != ST_STRUCT) {
			if (print_err) {
				char* full_ident = expr_ident_get_full_lexeme(dt->named.ident);
				Defer(buf_free(full_ident));
				error_expr(
						self,
						dt->named.ident,
						ERROR_IS_NOT_A_VALID_TYPE,
						full_ident);
				note_token(
						self, 
						dt->named.ref->ident,
						NOTE_DEFINED_HERE);
			} else {
				set_error_flags(self);
			}
			return true;
		}
		return false;
	} else if (dt->ty == DT_STRUCT) {
		// TODO
		return false;
	}
	assert(0);
	return false;
}

static DataType* expr_ident(Checker* self, Expr* e) {Deferral
	if (e->ident.ref->ty != ST_VARIABLE && e->ident.ref->ty != ST_FUNCTION) {	// TODO: print static accessor with identifier name
		char* full_ident = expr_ident_get_full_lexeme(e);
		Defer(buf_free(full_ident));
		error_expr(
				self, 
				e,
				ERROR_IS_A,
				full_ident,
				one_word_stmt_ty(e->ident.ref));
		note_token(
				self, 
				e->ident.ref->ident,
				NOTE_DEFINED_HERE);
		return null;
	}
	else if (e->ident.ref->ty == ST_VARIABLE) {
		if (e->ident.ref->variable.variable->dt && !data_type(self, e->ident.ref->variable.variable->dt, false)) {
			return e->ident.ref->variable.variable->dt;
		} else {
			set_error_flags(self);
		}
	}
	else if (e->ident.ref->ty == ST_FUNCTION) {
		if (e->ident.ref->function.header->return_data_type && !data_type(self, e->ident.ref->function.header->return_data_type, false)) {
			return e->ident.ref->function.header->return_data_type;
		} else {
			set_error_flags(self);
		}
	}
	return null;
}

static DataType* expr_block(Checker* self, Expr* e) {
	buf_loop(e->block.stmts, s) {
		stmt(self, e->block.stmts[s]);
	}
	return null;
}

static DataType* expr_function_call(Checker* self, Expr* e) {
	return expr_ident(self, e->function_call.left);
}

static DataType* expr_assign(Checker* self, Expr* e) {Deferral
	DataType* left_type = null;
	DataType* right_type = null;
	chk((
			left_type = expr(self, e->assign.left),
			right_type = expr(self, e->assign.right)));
	if (!can_data_types_coerce(left_type, right_type)) {
		char* left_type_str = data_type_to_string(left_type);
		Defer(buf_free(left_type_str));
		char* right_type_str = data_type_to_string(right_type);
		Defer(buf_free(right_type_str));
		error_token(
				self, 
				e->assign.right->head,
				ERROR_CANNOT_ASSIGN_VARIABLE_OF_TYPE,
				left_type_str,
				right_type_str);
		return null;
	}
	return left_type;
}

static DataType* expr(Checker* self, Expr* e) {
	switch (e->ty) {
		case ET_IDENT:
			return expr_ident(self, e);
		case ET_BLOCK: 
			return expr_block(self, e);
		case ET_FUNCTION_CALL:
			return expr_function_call(self, e);
		case ET_ASSIGN: 
			return expr_assign(self, e); 
	}
	assert(0);
	return false;
}

static void stmt_namespace(Checker* self, Stmt* s) {
	buf_loop(s->namespace_.stmts, i) {
		stmt(self, s->namespace_.stmts[i]);
	}
}

static void stmt_function(Checker* self, Stmt* s) {
	expr(self, s->function.body);
}

static void stmt_variable(Checker* self, Stmt* s) {Deferral
	if (s->variable.variable->dt) {
		if (data_type(self, s->variable.variable->dt, true)) {
			return;
		}
	}

	if (!s->variable.variable->dt) {
		s->variable.variable->dt = expr(self, s->variable.variable->initializer);
	} else if (s->variable.variable->dt && s->variable.variable->initializer) {
		DataType* initializer_type = null;
	   	chkv(initializer_type = expr(self, s->variable.variable->initializer));
		if (!can_data_types_coerce(s->variable.variable->dt, initializer_type)) {
			char* dt_str = data_type_to_string(s->variable.variable->dt);
			Defer(buf_free(dt_str));
			char* initializer_type_str = data_type_to_string(initializer_type);
			Defer(buf_free(initializer_type_str));
			// TODO: if DT_STRUCT then error_token else if DT_NAMED error_expr
			if (s->variable.variable->dt->ty == DT_NAMED) {
				error_expr(
						self,
						s->variable.variable->dt->named.ident,
						ERROR_ANNOTATED_INFERRED_INITIALIZER_MISMATCH,
						dt_str,
						initializer_type_str);
			}
			return;
		}
	}
}

static void stmt_expr(Checker* self, Stmt* s) {
	expr(self, s->expr);
}

static void stmt(Checker* self, Stmt* s) {
	switch (s->ty) {
		case ST_NAMESPACE: 
			stmt_namespace(self, s);
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

void checker_init(Checker* self, SrcFile* srcfile) {
	self->srcfile = srcfile;
	self->error = false;
	self->error_count = 0;
}

void checker_check(Checker* self) {
	buf_loop(self->srcfile->stmts, s) {
		stmt(self, self->srcfile->stmts[s]);
	}
}
