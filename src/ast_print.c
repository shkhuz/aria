#include <aria_core.h>
#include <aria.h>

#define print_lparen() printf("(")
#define print_rparen() printf(")")
#define print_space() printf(" ")
#define print_newline() printf("\n")
#define print_tok(tok) printf("%s", tok->lexeme)

static void data_type(AstPrinter* self, DataType* dt);
static void expr(AstPrinter* self, Expr* e);

static void print_indent(AstPrinter* self) {
	for (int i = 0; i < self->indent; i++) {
		for (int t = 0; t < AST_TAB_COUNT; t++) {
			print_space();
		}
	}
}

static void data_type_named(AstPrinter* self, DataType* dt) {
	print_tok(dt->named.ident);
}

static void variable(AstPrinter* self, Variable* v) {
	print_lparen();

	print_tok(v->ident);
	printf(": ");
	if (v->dt) {
		data_type(self, v->dt);
	}

	if (v->initializer) {
		printf(" = ");
		expr(self, v->initializer);
	}

	print_rparen();
}

static void data_type_struct(AstPrinter* self, DataType* dt) {
	print_lparen();
	printf("struct ");
	if (dt->struct_.ident) {
		print_tok(dt->struct_.ident);
	} else {
		printf("<null>");
	}
	print_newline();

	self->indent++;
	buf_loop(dt->struct_.fields, f) {
		print_indent(self);
		variable(self, dt->struct_.fields[f]);
		print_newline();
	}
	self->indent--;

	print_indent(self);
	print_rparen();
}

static void data_type(AstPrinter* self, DataType* dt) {
	switch (dt->ty) {
		case DT_NAMED:
			data_type_named(self, dt);
			break;
		case DT_STRUCT:
			data_type_struct(self, dt);
			break;
	}
}

static void expr_ident(AstPrinter* self, Expr* e) {
	print_tok(e->ident);
}

static void expr_binary(AstPrinter* self, Expr* e) {
	print_lparen();
	print_tok(e->binary.op);
	print_space();
	expr(self, e->binary.left);
	print_space();
	expr(self, e->binary.right);
	print_rparen();
}

static void expr(AstPrinter* self, Expr* e) {
	switch (e->ty) {
		case ET_IDENT: 
			expr_ident(self, e);
			break;
		case ET_BINARY_ADD:
		case ET_BINARY_SUBTRACT:
		case ET_BINARY_MULTIPLY:
		case ET_BINARY_DIVIDE:
			expr_binary(self, e);
			break;
		case ET_NONE: 
			break;
	}
}

static void stmt_struct(AstPrinter* self, Stmt* s) {
	data_type(self, s->struct_);
}

static void stmt_variable(AstPrinter* self, Stmt* s) {
	variable(self, &s->variable);
}

static void stmt_expr(AstPrinter* self, Stmt* s) {
	print_lparen();
	printf("stmt-expr ");
	expr(self, s->expr);
	print_rparen();
}

static void stmt(AstPrinter* self, Stmt* s) {
	switch (s->ty) {
		case ST_STRUCT:
			stmt_struct(self, s);
			break;
		case ST_VARIABLE:
			stmt_variable(self, s);
			break;
		case ST_EXPR:
			stmt_expr(self, s);
			break;
		case ST_NONE: 
			break;
	}
	print_newline();
}

void ast_printer_init(AstPrinter* self, SrcFile* srcfile) {
	self->srcfile = srcfile;
	self->indent = 0;
}

void ast_printer_print(AstPrinter* self) {
	buf_loop(self->srcfile->stmts, s) {
		stmt(self, self->srcfile->stmts[s]);
	}
}
