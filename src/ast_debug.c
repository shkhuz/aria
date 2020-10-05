#include "arpch.h"
#include "ds/ds.h"
#include "util/util.h"
#include "aria.h"

#define print_space(self) print_chr(self, ' ')
#define print_newline(self) print_chr(self, '\n')
#define print_l_paren(self) print_chr(self, '(')
#define print_r_paren(self) print_chr(self, ')')

static void print_chr(AstDebugger* self, char c) {
    printf("%c", c);
}

static void print_str(AstDebugger* self, const char* str) {
    printf("%s", str);
}

static void print_token(AstDebugger* self, Token* token) {
    print_str(self, token->lexeme);
}

static void print_data_type(AstDebugger* self, DataType* dt) {
    print_token(self, dt->identifier);
    for (u8 p = 0; p < dt->pointer_count; p++) {
        print_chr(self, '*');
    }
}

static void print_tab(AstDebugger* self) {
    for (u64 t = 0; t < TAB_COUNT; t++) {
        print_space(self);
    }
}

static void indent(AstDebugger* self) {
    for (u64 i = 0; i < self->indent; i++) {
        print_tab(self);
    }
}

static void dbg_stmt(AstDebugger* self, Stmt* stmt);
static void dbg_expr(AstDebugger* self, Expr* expr);

static void dbg_binary_expr(AstDebugger* self, Expr* expr) {
    dbg_expr(self, expr->binary.left);
    print_space(self);
    print_token(self, expr->binary.op);
    print_space(self);
    dbg_expr(self, expr->binary.right);
}

static void dbg_func_call_expr(AstDebugger* self, Expr* expr) {
    print_str(self, "call ");
    dbg_expr(self, expr->func_call.left);
    print_space(self);
    buf_loop(expr->func_call.args, a) {
        dbg_expr(self, expr->func_call.args[a]);
        if (a != buf_len(expr->func_call.args)-1) {
            print_space(self);
        }
    }
}

static void dbg_integer_expr(AstDebugger* self, Expr* expr) {
    print_token(self, expr->integer);
}

static void dbg_variable_ref(AstDebugger* self, Expr* expr) {
    print_token(self, expr->variable_ref.identifier);
}

static void dbg_block_expr(AstDebugger* self, Expr* expr) {
    print_str(self, "block ");
    self->indent++;
    buf_loop(expr->block.stmts, s) {
        dbg_stmt(self, expr->block.stmts[s]);
    }
    if (expr->block.ret) {
        print_newline(self);
        indent(self);
        print_str(self, "<- ");
        dbg_expr(self, expr->block.ret);
    }
    self->indent--;
}

static void dbg_expr(AstDebugger* self, Expr* expr) {
    print_l_paren(self);
    switch (expr->type) {
    case E_BINARY: dbg_binary_expr(self, expr); break;
    case E_FUNC_CALL: dbg_func_call_expr(self, expr); break;
    case E_INTEGER: dbg_integer_expr(self, expr); break;
    case E_VARIABLE_REF: dbg_variable_ref(self, expr); break;
    case E_BLOCK: dbg_block_expr(self, expr); break;
    }
    print_r_paren(self);
}

static void dbg_function(AstDebugger* self, Stmt* stmt) {
    print_l_paren(self);
    print_str(self, "function ");
    print_token(self, stmt->function.identifier);
    print_space(self);

    print_l_paren(self);
    Stmt** params = stmt->function.params;
    buf_loop(params, p) {
        print_l_paren(self);
        print_token(self, params[p]->variable_decl.identifier);
        print_str(self, ": ");
        print_data_type(self, params[p]->variable_decl.data_type);
        print_r_paren(self);
        if (p != buf_len(params)-1) print_space(self);
    }
    print_r_paren(self);
    print_space(self);
    print_data_type(self, stmt->function.return_type);
    print_space(self);

    dbg_expr(self, stmt->function.block);
    print_r_paren(self);
}

static void dbg_variable_decl(AstDebugger* self, Stmt* stmt) {
    print_l_paren(self);
    print_str(self, "variable ");
    print_token(self, stmt->variable_decl.identifier);

    if (stmt->variable_decl.data_type) {
        print_str(self, ": ");
        print_data_type(self, stmt->variable_decl.data_type);
    }

    if (stmt->variable_decl.initializer) {
        print_space(self);
        dbg_expr(self, stmt->variable_decl.initializer);
    }

    print_r_paren(self);
}

static void dbg_return_stmt(AstDebugger* self, Stmt* stmt) {
    print_l_paren(self);
    print_str(self, "return");
    if (stmt->ret.expr) {
        print_space(self);
        dbg_expr(self, stmt->ret.expr);
    }
    print_r_paren(self);
}

static void dbg_expr_stmt(AstDebugger* self, Stmt* stmt) {
    dbg_expr(self, stmt->expr);
}

static void dbg_stmt(AstDebugger* self, Stmt* stmt) {
    if (!self->first_stmt) {
        print_newline(self);
    }
    self->first_stmt = false;

    indent(self);
    switch (stmt->type) {
    case S_FUNCTION: dbg_function(self, stmt); break;
    case S_VARIABLE_DECL: dbg_variable_decl(self, stmt); break;
    case S_RETURN: dbg_return_stmt(self, stmt); break;
    case S_EXPR: dbg_expr_stmt(self, stmt); break;
    }
}

void ast_debug(AstDebugger* self, Ast* ast) {
    self->ast = ast;
    self->indent = 0;
    self->first_stmt = true;

    buf_loop(self->ast->stmts, s) {
        dbg_stmt(self, self->ast->stmts[s]);
    }
    print_newline(self);
}
