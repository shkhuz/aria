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

#define r_paren() \
    chr(')')

#define spc() \
    chr(' ')

#define comma() \
    chr(',')

#define colon() \
    chr(':')

#define newline() \
    chr('\n')

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

static void format_nh(void) {
    newline();
    indent_count++;
}

static void format_ret_nh(void) {
    newline();
    indent_count--;
}

static void token(Token* token) {
    string(token->lexeme);
}

static void data_type(DataType* data_type) {
    token(data_type->identifier);
    for (u8 p = 0; p < data_type->pointer_count; p++) {
        chr('*');
    }
}

static void expr(Expr* expr);

static void expr_integer(Expr* e) {
    token(e->e.integer);
}

static void expr_string(Expr* e) {
    token(e->e.str);
}

static void expr_char(Expr* e) {
    token(e->e.chr);
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
    case E_STRING: expr_string(expr); break;
    case E_CHAR: expr_char(expr); break;
    case E_NONE: assert(0); break;
    default: assert(0); break;
    }
}

static void stmt(Stmt* stmt);

static void stmt_struct(Stmt* stmt) {
    string("STMT_STRUCT ");
    token(stmt->s.struct_decl.identifier);
    format();

    Stmt** fields = stmt->s.struct_decl.fields;
    buf_loop(fields, f) {
        token(fields[f]->s.variable_decl.identifier);
        string(": ");
        data_type(fields[f]->s.variable_decl.data_type);

        newline();
        tab_indent();
    }
    format_ret();

}

static void param(Stmt* param) {
    token(param->s.variable_decl.identifier);
    colon(); spc();
    data_type(param->s.variable_decl.data_type);
    comma(); spc();
}

static void stmt_function(Stmt* s) {
    string("STMT_FUNCTION");
    if (s->s.function.decl) string("(decl) ");
    else string("(def) ");
    token(s->s.function.identifier); spc();
    l_paren();
    buf_loop(s->s.function.params, p) {
        param(s->s.function.params[p]);
    }
    r_paren();
    spc(); data_type(s->s.function.return_type);

    if (!s->s.function.decl) {
        format_nh();
        stmt(s->s.function.body);
        format_ret_nh();
    }
    else newline();
}

static void stmt_variable_decl(Stmt* s) {
    string("STMT_VARIABLE_DECL");
    if (s->s.variable_decl.external) string("(extern) ");
    else chr(' ');

    token(s->s.variable_decl.identifier);
    if (s->s.variable_decl.data_type) {
        spc(); data_type(s->s.variable_decl.data_type);
    }
    if (s->s.variable_decl.initializer) {
        string(" = "); expr(s->s.variable_decl.initializer);
    }
    newline();
}

static void stmt_block(Stmt* s) {
    for (u64 e = 0; e < buf_len(s->s.block); e++) {
        stmt(s->s.block[e]);
    }
}

static void stmt_expr(Stmt* s) {
    string("STMT_EXPR ");
    format();
    expr(s->s.expr);
    format_ret();
}

static void stmt(Stmt* stmt) {
    if (stmt->type != S_BLOCK) {
        tab_indent();
    }

    switch (stmt->type) {
    case S_STRUCT: stmt_struct(stmt); break;
    case S_FUNCTION: stmt_function(stmt); break;
    case S_VARIABLE_DECL: stmt_variable_decl(stmt); break;
    case S_BLOCK: stmt_block(stmt); break;
    case S_EXPR: stmt_expr(stmt); break;
    default: assert(0); break;
    }
}

void print_ast(Stmt** stmts) {
    for (u64 s = 0; s < buf_len(stmts); s++) {
        stmt(stmts[s]);
    }
}
