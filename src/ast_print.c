#include <ast_print.h>
#include <token.h>
#include <expr.h>
#include <stmt.h>
#include <error_msg.h>
#include <arpch.h>

static int indent_count = 0;

#define string(str) \
    printf("%s", str)

#define chr(c) \
    fputc(c, stdout)

#define l_paren() \
    chr('(')

/* TODO: complete this */

static void r_paren(void) {
    chr(')');
}

static void newline(void) {
    chr('\n');
}

static void tab(void) {
    for (int i = 0; i < AST_TAB_COUNT; i++) chr(' ');
}

static void tab_indent(void) {
    for (int i = 0; i < indent_count; i++) tab();
}

static void format(void) {
    newline();
    indent_count++;
    tab_indent();
}

static void format_ret(void) {
    newline();
    indent_count--;
    tab_indent();
}

static void token(Token* token) {
    string(token->lexeme);
}

static void expr(Expr* expr);

static void expr_integer(Expr* e) {
    token(e->e.integer);
}

static void expr_unary(Expr* e) {
    l_paren();
    token(e->e.unary.op);
    expr(e->e.unary.right);
    r_paren();
}

static void expr_binary(Expr* e) {
    l_paren();
    format();
    expr(e->e.binary.left);
    token(e->e.binary.op);
    expr(e->e.binary.right);
    format_ret();
    r_paren();
}

static void expr(Expr* expr) {
    switch (expr->type) {
        case E_BINARY: expr_binary(expr); break;
        case E_UNARY: expr_unary(expr); break;
        case E_INTEGER: expr_integer(expr); break;
        case E_NONE: assert(0); break;
    }
}

static void stmt_expr(Stmt* s) {
    string("STMT_EXPR ");
    format();
    expr(s->s.expr);
    format_ret();
}

static void stmt(Stmt* stmt) {
    switch (stmt->type) {
        case S_EXPR: stmt_expr(stmt); break;
        case S_NONE: assert(0); break;
    }
}

void print_ast(Stmt** stmts) {
    for (u64 s = 0; s < buf_len(stmts); s++) {
        stmt(stmts[s]);
    }
}
