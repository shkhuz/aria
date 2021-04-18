#include <aria_core.h>
#include <aria.h>

#define print_lparen() printf("(")
#define print_rparen() printf(")")
#define print_space() printf(" ")
#define print_newline() printf("\n")
#define print_tok(tok) printf("%s", tok->lexeme)

static void expr(AstPrinter* self, Expr* e);

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

static void stmt_expr(AstPrinter* self, Stmt* s) {
	print_lparen();
	printf("stmt-expr ");
	expr(self, s->expr);
	print_rparen();
	print_newline();
}

static void stmt(AstPrinter* self, Stmt* s) {
	switch (s->ty) {
		case ST_EXPR:
			stmt_expr(self, s);
			break;
		case ST_NONE: 
			break;
	}
}

void ast_printer_init(AstPrinter* self, SrcFile* srcfile) {
	self->srcfile = srcfile;
}

void ast_printer_print(AstPrinter* self) {
	buf_loop(self->srcfile->stmts, s) {
		stmt(self, self->srcfile->stmts[s]);
	}
}
