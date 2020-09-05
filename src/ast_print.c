#include <ast_print.h>
#include <token.h>
#include <expr.h>
#include <error_msg.h>
#include <arpch.h>

static int indent_count = 0;

static void string(const char* string) {
    printf("%s", string);
}

static void chr(char chr) {
    fputc(chr, stdout);
}

static void l_paren(void) {
    chr('(');
}

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

static void expr_integer(Expr* expr) {
    token(expr->e.integer);
}

static void expr_unary(Expr* expr) {
    l_paren();
    token(expr->e.unary.op);
    print_expr(expr->e.unary.right);
    r_paren();
}

static void expr_binary(Expr* expr) {
    l_paren();
    format();
    print_expr(expr->e.binary.left);
    token(expr->e.binary.op);
    print_expr(expr->e.binary.right);
    format_ret();
    r_paren();
}

void print_expr(Expr* expr) {
    switch (expr->type) {
        case E_BINARY: expr_binary(expr); break;
        case E_UNARY: expr_unary(expr); break;
        case E_INTEGER: expr_integer(expr); break;
        case E_NONE: assert(0);
    }
}
