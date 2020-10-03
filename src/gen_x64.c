#include "arpch.h"
#include "util/util.h"
#include "error_msg.h"
#include "ds/ds.h"
#include "aria.h"

#define gen_l_paren(self) gen_chr(self, '(')
#define gen_r_paren(self) gen_chr(self, ')')
#define gen_colon(self) gen_chr(self, ':')
#define gen_semicolon(self) gen_chr(self, ';')
#define gen_newline(self) gen_chr(self, '\n')
#define gen_space(self) gen_chr(self, ' ')

static void gen_chr(CodeGenerator* self, char c) {
    buf_push(self->code, c);
}

static void gen_str(CodeGenerator* self, char* str) {
    u64 len = strlen(str);
    for (u64 i = 0; i < len; i++) {
        gen_chr(self, str[i]);
    }
}

static void gen_tab(CodeGenerator* self) {
    for (u64 c = 0; c < TAB_COUNT; c++) {
        gen_space(self);
    }
}

static void gen_token(CodeGenerator* self, Token* token) {
    gen_str(self, token->lexeme);
}

/* asm write (w/o fmt) */
static void asmw(CodeGenerator* self, const char* str) {
    gen_tab(self);
    gen_str(self, (char*)str);
    gen_newline(self);
}

static char _asmp_buffer[512];

/* asm print (w/ fmt) */
static void asmp(CodeGenerator* self, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    size_t bytes_written =
        vsnprintf(_asmp_buffer, sizeof(_asmp_buffer), fmt, ap);
    assert(bytes_written < 511);
    _asmp_buffer[bytes_written] = 0;
    asmw(self, _asmp_buffer);
    va_end(ap);
}

static void label(CodeGenerator* self, const char* identifier) {
    gen_str(self, (char*)identifier);
    gen_colon(self);
    gen_newline(self);
}

static void label_from_token(CodeGenerator* self, Token* token) {
    label(self, token->lexeme);
}

static void gen_prelude(CodeGenerator* self) {
    asmw(self, "section .text");
}

/* register push/pop macros */
#define push_rax(self) asmw(self, "push rax")
#define pop_rbx(self) asmw(self, "pop rbx")

#define DT_SIZE_POINTER 8
#define DT_SIZE_CHAR    1
#define DT_SIZE_U8      1
#define DT_SIZE_U16     2
#define DT_SIZE_U32     4
#define DT_SIZE_U64     8
#define DT_SIZE_I8      1
#define DT_SIZE_I16     2
#define DT_SIZE_I32     4
#define DT_SIZE_I64     8

static u64 get_data_type_size(CodeGenerator* self, DataType* dt) {
    if (dt->pointer_count) {
        return DT_SIZE_POINTER;
    }

    if (str_intern(dt->identifier->lexeme) == str_intern("void"))
        return 0;
    if (str_intern(dt->identifier->lexeme) == str_intern("char"))
        return DT_SIZE_CHAR;

    if (str_intern(dt->identifier->lexeme) == str_intern("u8"))
        return DT_SIZE_U8;
    if (str_intern(dt->identifier->lexeme) == str_intern("u16"))
        return DT_SIZE_U16;
    if (str_intern(dt->identifier->lexeme) == str_intern("u32"))
        return DT_SIZE_U32;
    if (str_intern(dt->identifier->lexeme) == str_intern("u64"))
        return DT_SIZE_U64;

    if (str_intern(dt->identifier->lexeme) == str_intern("i8"))
        return DT_SIZE_I8;
    if (str_intern(dt->identifier->lexeme) == str_intern("i16"))
        return DT_SIZE_I16;
    if (str_intern(dt->identifier->lexeme) == str_intern("i32"))
        return DT_SIZE_I32;
    if (str_intern(dt->identifier->lexeme) == str_intern("i64"))
        return DT_SIZE_I64;
    assert(0);
}

#define DT_NASM_TYPE_SPECIFIER_BYTE "byte"
#define DT_NASM_TYPE_SPECIFIER_WORD "word"
#define DT_NASM_TYPE_SPECIFIER_DWORD "dword"
#define DT_NASM_TYPE_SPECIFIER_QWORD "qword"

static const char* get_nasm_type_specifier(CodeGenerator* self, DataType* dt) {
    assert(!is_dt_eq(dt, builtin_types.e.void_type));

    if (dt->pointer_count) {
        return DT_NASM_TYPE_SPECIFIER_QWORD;
    }

    if (str_intern(dt->identifier->lexeme) == str_intern("char"))
        return DT_NASM_TYPE_SPECIFIER_BYTE;

    if (str_intern(dt->identifier->lexeme) == str_intern("u8"))
        return DT_NASM_TYPE_SPECIFIER_BYTE;
    if (str_intern(dt->identifier->lexeme) == str_intern("u16"))
        return DT_NASM_TYPE_SPECIFIER_WORD;
    if (str_intern(dt->identifier->lexeme) == str_intern("u32"))
        return DT_NASM_TYPE_SPECIFIER_DWORD;
    if (str_intern(dt->identifier->lexeme) == str_intern("u64"))
        return DT_NASM_TYPE_SPECIFIER_QWORD;

    if (str_intern(dt->identifier->lexeme) == str_intern("i8"))
        return DT_NASM_TYPE_SPECIFIER_BYTE;
    if (str_intern(dt->identifier->lexeme) == str_intern("i16"))
        return DT_NASM_TYPE_SPECIFIER_WORD;
    if (str_intern(dt->identifier->lexeme) == str_intern("i32"))
        return DT_NASM_TYPE_SPECIFIER_DWORD;
    if (str_intern(dt->identifier->lexeme) == str_intern("i64"))
        return DT_NASM_TYPE_SPECIFIER_QWORD;
    assert(0);
}

static void move_rax_into_variable(
        CodeGenerator* self,
        VariableDeclOffset* var) {
    asmp(
            self,
            "mov %s [rbp - %lu], rax",
            get_nasm_type_specifier(self, var->stmt->variable_decl.data_type),
            var->offset
    );
}

static void move_variable_into_rax(
        CodeGenerator* self,
        VariableDeclOffset* var) {
    asmp(
            self,
            "mov rax, %s [rbp - %lu]",
            get_nasm_type_specifier(self, var->stmt->variable_decl.data_type),
            var->offset
    );
}

static void gen_stmt(CodeGenerator* self, Stmt* stmt);
static void gen_expr(CodeGenerator* self, Expr* expr);

static void gen_binary_arithmetic_expr(CodeGenerator* self, Expr* expr) {
    gen_expr(self, expr->binary.right);
    push_rax(self);
    gen_expr(self, expr->binary.left);
    pop_rbx(self);

    const char* operation_opcode = "";
    switch (expr->binary.op->type) {
    case T_PLUS: operation_opcode = "add"; break;
    case T_MINUS: operation_opcode = "sub"; break;
    default: assert(0); break;
    }

    asmp(self, "%s rax, rbx", operation_opcode);
}

static void gen_binary_expr(CodeGenerator* self, Expr* expr) {
    switch (expr->binary.op->type) {
    case T_PLUS:
    case T_MINUS: gen_binary_arithmetic_expr(self, expr); break;
    default: break;
    }
}

static void gen_integer_expr(CodeGenerator* self, Expr* expr) {
    asmp(self, "mov rax, %s", expr->integer->lexeme);
}

static void gen_variable_ref(CodeGenerator* self, Expr* expr) {
    VariableDeclOffset** vars =
        self->enclosed_function->function.variable_decls;
    buf_loop(vars, v) {
        if (is_tok_eq(vars[v]->stmt->variable_decl.identifier,
                      expr->variable_ref.identifier)) {
            move_variable_into_rax(self, vars[v]);
        }
    }
}

static void gen_expr(CodeGenerator* self, Expr* expr) {
    switch (expr->type) {
    case E_BINARY: gen_binary_expr(self, expr); break;
    case E_INTEGER: gen_integer_expr(self, expr); break;
    case E_VARIABLE_REF: gen_variable_ref(self, expr); break;
    }
}

static void gen_function(CodeGenerator* self, Stmt* stmt) {
    self->enclosed_function = stmt;
    if (stmt->function.pub) {
        asmp(self, "global %s", stmt->function.identifier->lexeme);
    }
    label_from_token(self, stmt->function.identifier);
    asmw(self, "push rbp");
    asmw(self, "mov rbp, rsp");

    u64 resv_local_variables_stack_space = 0;
    VariableDeclOffset** variable_decls = stmt->function.variable_decls;
    buf_loop(variable_decls, v) {
        resv_local_variables_stack_space +=
            get_data_type_size(
                    self,
                    variable_decls[v]->stmt->variable_decl.data_type
            );
    }
    /* align to 16-bytes for amd64 calling convention */
    resv_local_variables_stack_space =
        math_round_up(resv_local_variables_stack_space, 16);
    /* printf( */
    /*         "(%s) stack space: %lu\n", */
    /*         stmt->function.identifier->lexeme, */
    /*         resv_local_variables_stack_space */
    /* ); */
    if (resv_local_variables_stack_space != 0) {
        asmp(self, "sub rsp, %lu", resv_local_variables_stack_space);
    }

    buf_loop(stmt->function.block->block.stmts, s) {
        gen_stmt(self, stmt->function.block->block.stmts[s]);
    }
    self->enclosed_function = null;

    label(self, ".return");
    asmw(self, "leave");
    asmw(self, "ret");
    asmw(self, "");
}

static void gen_variable_decl(CodeGenerator* self, Stmt* stmt) {
    VariableDeclOffset** vars =
        self->enclosed_function->function.variable_decls;
    VariableDeclOffset* cur_var;
    buf_loop(vars, v) {
        if (is_tok_eq(vars[v]->stmt->variable_decl.identifier,
                      stmt->variable_decl.identifier)) {
            vars[v]->offset = self->next_local_var_offset;
            self->next_local_var_offset +=
                get_data_type_size(self, stmt->variable_decl.data_type);
            cur_var = vars[v];
        }
    }

    if (stmt->variable_decl.initializer) {
        gen_expr(self, stmt->variable_decl.initializer);
        move_rax_into_variable(self, cur_var);
    }
}

static void gen_return_stmt(CodeGenerator* self, Stmt* stmt) {
    gen_expr(self, stmt->ret.expr);
    asmw(self, "jmp .return");
}

static void gen_expr_stmt(CodeGenerator* self, Stmt* stmt) {
    gen_expr(self, stmt->expr);
}

static void gen_stmt(CodeGenerator* self, Stmt* stmt) {
    switch (stmt->type) {
    case S_FUNCTION: gen_function(self, stmt); break;
    case S_VARIABLE_DECL: gen_variable_decl(self, stmt); break;
    case S_RETURN: gen_return_stmt(self, stmt); break;
    case S_EXPR: gen_expr_stmt(self, stmt); break;
    }
}

void gen_code_for_ast(CodeGenerator* self, Ast* ast, const char* fpath) {
    self->ast = ast;
    self->enclosed_function = null;
    self->code = null;
    self->next_local_var_offset = 8;

    gen_prelude(self);
    buf_loop(self->ast->stmts, s) {
        gen_stmt(self, self->ast->stmts[s]);
    }
    buf_push(self->code, '\0');

    FILE* file = fopen(fpath, "w");
    fwrite(self->code, sizeof(char), buf_len(self->code) - 1, file);
    fclose(file);
    printf("%s", self->code);
}
