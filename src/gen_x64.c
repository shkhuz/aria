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
    Stmt** stmts = self->ast->stmts;
    buf_loop(self->ast->func_sym_tbl, s) {
        if (stmts[s]->type == S_FUNCTION &&
            stmts[s]->function.decl) {
            asmp(self, "extern %s", stmts[s]->function.identifier->lexeme);
        }
    }
}

/* register push/pop macros */
#define push_rax(self) asmw(self, "push rax")
#define pop_rbx(self) asmw(self, "pop rbx")

#define DT_SIZE_8  1
#define DT_SIZE_16 2
#define DT_SIZE_32 4
#define DT_SIZE_64 8
#define DT_SIZE_POINTER DT_SIZE_64
#define DT_SIZE_CHAR    DT_SIZE_8
#define DT_SIZE_U8      DT_SIZE_8
#define DT_SIZE_U16     DT_SIZE_16
#define DT_SIZE_U32     DT_SIZE_32
#define DT_SIZE_U64     DT_SIZE_64
#define DT_SIZE_I8      DT_SIZE_8
#define DT_SIZE_I16     DT_SIZE_16
#define DT_SIZE_I32     DT_SIZE_32
#define DT_SIZE_I64     DT_SIZE_64

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

const char* param_registers[6] = {
    "rdi",
    "rsi",
    "rdx",
    "rcx",
    "r8",
    "r9"
};

static void move_rax_into_variable(
        CodeGenerator* self,
        Stmt* var) {
    if (is_dt_eq(var->variable_decl.data_type, builtin_types.e.void_type)) {
        return;
    }

    u64 var_offset = var->variable_decl.offset;
    char* acc_reg = "rax";
    DataType* dt = var->variable_decl.data_type;
    if (is_dt_eq(dt, builtin_types.e.u8_type) ||
        is_dt_eq(dt, builtin_types.e.i8_type)) {
        acc_reg = "al";
    }
    else if (is_dt_eq(dt, builtin_types.e.u16_type) ||
             is_dt_eq(dt, builtin_types.e.i16_type)) {
        acc_reg = "ax";
    }
    else if (is_dt_eq(dt, builtin_types.e.u32_type) ||
             is_dt_eq(dt, builtin_types.e.i32_type)) {
        acc_reg = "eax";
    }

    if (var->variable_decl.param) {
        if (var_offset > 5) {
            /* on stack */
            asmp(
                    self,
                    "mov %s [rbp + %lu], %s",
                    get_nasm_type_specifier(self, var->variable_decl.data_type),
                    var_offset,
                    acc_reg
            );
        }
        else {
            /* in reg */
            const char* reg_in = param_registers[var_offset];
            asmp(self, "mov %s, rax", reg_in);
        }
    }
    else {
        asmp(
                self,
                "mov %s [rbp - %lu], %s  ; %s",
                get_nasm_type_specifier(self, var->variable_decl.data_type),
                var_offset,
                acc_reg,
                var->variable_decl.identifier->lexeme
        );
    }
}

static void move_variable_into_rax(
        CodeGenerator* self,
        Stmt* var) {
    if (is_dt_eq(var->variable_decl.data_type, builtin_types.e.void_type)) {
        return;
    }

    u64 var_offset = var->variable_decl.offset;
    char* inst_opcode = "mov";
    char* acc_reg = "rax";
    bool cdqe = false;
    DataType* dt = var->variable_decl.data_type;
    if (is_dt_eq(dt, builtin_types.e.u8_type) ||
        is_dt_eq(dt, builtin_types.e.u16_type)) {
        inst_opcode = "movzx";
    }
    else if (is_dt_eq(dt, builtin_types.e.u32_type)) {
        acc_reg = "eax";
    }
    else if (is_dt_eq(dt, builtin_types.e.i8_type) ||
        is_dt_eq(dt, builtin_types.e.i16_type)) {
        inst_opcode = "movzx";
    }
    else if (is_dt_eq(dt, builtin_types.e.i32_type)) {
        /* sign-extend dword to qword */
        cdqe = true;
    }
    /* no computation for qwords */

    if (var->variable_decl.param) {
        if (var_offset > 5) {
            /* on stack */
            asmp(
                    self,
                    "%s %s, %s [rbp + %lu]",
                    inst_opcode,
                    acc_reg,
                    get_nasm_type_specifier(self, var->variable_decl.data_type),
                    var_offset
            );
        }
        else {
            /* in reg */
            const char* reg_in = param_registers[var_offset];
            asmp(self, "mov rax, %s", reg_in);
        }
    }
    else {
        asmp(
                self,
                "%s %s, %s [rbp - %lu]  ; %s",
                inst_opcode,
                acc_reg,
                get_nasm_type_specifier(self, var->variable_decl.data_type),
                var_offset,
                var->variable_decl.identifier->lexeme
        );
    }
    if (cdqe) {
        asmw(self, "cdqe");
    }
}

static void move_variable_addr_into_rax(
        CodeGenerator* self,
        Stmt* var) {
    if (is_dt_eq(var->variable_decl.data_type, builtin_types.e.void_type)) {
        assert(0);
        return;
    }

    if (var->variable_decl.param) {
        u64 var_offset = var->variable_decl.offset;
        if (var_offset > 5) {
            /* on stack */
            asmp(
                    self,
                    "lea rax, [rbp + %lu]",
                    var_offset
            );
        }
        else {
            /* in reg */
            const char* reg_in = param_registers[var_offset];
            asmp(self, "lea rax, [%s]", reg_in);
        }
    }
    else {
        asmp(
                self,
                "lea rax, [rbp - %lu]",
                var->variable_decl.offset
        );
    }
}

static char* generate_next_free_string_label(CodeGenerator* self) {
    const char* label_fmt = "__aria_static_str_%lu";
    size_t need_len = snprintf(
            null,
            0,
            label_fmt,
            self->next_free_string_label
    );
    char* label = malloc(need_len + 1);
    sprintf(
            label,
            label_fmt,
            self->next_free_string_label
    );
    label[need_len] = 0;
    self->next_free_string_label++;
    return label;
}

/* returns asm label */
static char* static_strings_contains(CodeGenerator* self, char* check) {
    buf_loop(self->static_strings, s) {
        if (str_intern(self->static_strings[s]->b) ==
            str_intern(check)) {
            return self->static_strings[s]->a;
        }
    }
    return null;
}

static char* static_strings_push(CodeGenerator* self, char* str) {
    TwoStringMap* map = malloc(sizeof(*map));
    map->a = generate_next_free_string_label(self);
    map->b = str_intern(str);
    buf_push(self->static_strings, map);
    return map->a;
}

static char* static_strings_get_label_for_str(CodeGenerator* self, char* str) {
    char* label = static_strings_contains(self, str);
    if (label) return label;
    return static_strings_push(self, str);
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

static void gen_assign_expr(CodeGenerator* self, Expr* expr) {
    gen_expr(self, expr->binary.right);
    move_rax_into_variable(self, expr->binary.left->variable_ref.declaration);
}

static void gen_binary_expr(CodeGenerator* self, Expr* expr) {
    switch (expr->binary.op->type) {
    case T_PLUS:
    case T_MINUS: gen_binary_arithmetic_expr(self, expr); break;
    case T_EQUAL: gen_assign_expr(self, expr); break;
    default: break;
    }
}

static void gen_ref_expr(CodeGenerator* self, Expr* expr) {
    move_variable_addr_into_rax(
            self,
            expr->unary.right->variable_ref.declaration
    );
}

static void gen_unary_expr(CodeGenerator* self, Expr* expr) {
    switch (expr->unary.op->type) {
    case T_AMPERSAND: return gen_ref_expr(self, expr); break;
    default: assert(0); break;
    }
}

static void gen_func_call_expr(CodeGenerator* self, Expr* expr) {
    /* call prologue */
    asmw(self, "push r10");
    asmw(self, "push r11");
    Expr** args = expr->func_call.args;
    u64 args_len = buf_len(args);
    for (u64 a = 0; a < (MIN(args_len, 6)); a++) {
        /* put in reg */
        asmp(
                self,
                "push %s",
                param_registers[a]
        );
    }

    for (u64 a = 0; a < (MIN(args_len, 6)); a++) {
        gen_expr(self, args[a]);
        asmp(
                self,
                "mov %s, rax",
                param_registers[a]
        );
    }
    if (args_len > 6) {
        /* a -> 6 because first 6 args are already generated */
        for (u64 a = args_len; a -- > 6;) {
            /* push on stack, loop .. right to left */
            gen_expr(self, args[a]);
            asmw(self, "push rax");
        }
    }

    asmp(self, "call %s", expr->func_call.callee->function.identifier->lexeme);

    /* call epilogue */
    if (args_len > 6) {
        for (u64 a = args_len; a -- > 6;) {
            /* pop from stack */
            asmw(self, "add rsp, 8");
        }
    }
    for (u64 a = (MIN(args_len, 6)); a-- > 0;) {
        /* put in reg */
        asmp(
                self,
                "pop %s",
                param_registers[a]
        );
    }
    asmw(self, "pop r11");
    asmw(self, "pop r10");
}

static void gen_integer_expr(CodeGenerator* self, Expr* expr) {
    asmp(self, "mov rax, %s", expr->integer->lexeme);
}

static void gen_variable_ref(CodeGenerator* self, Expr* expr) {
    move_variable_into_rax(self, expr->variable_ref.declaration);
}

static void gen_string_expr(CodeGenerator* self, Expr* expr) {
    char* label =
        static_strings_get_label_for_str(
                self,
                str_intern_range(
                    expr->string->start + 1,
                    expr->string->end - 1
                )
        );
    asmp(self, "mov rax, %s", label);
}

static void gen_block_expr(CodeGenerator* self, Expr* expr) {
    buf_loop(expr->block.stmts, s) {
        gen_stmt(self, expr->block.stmts[s]);
    }
    if (expr->block.ret) {
        gen_expr(self, expr->block.ret);
    }
}

static void gen_expr(CodeGenerator* self, Expr* expr) {
    switch (expr->type) {
    case E_BINARY: gen_binary_expr(self, expr); break;
    case E_UNARY: gen_unary_expr(self, expr); break;
    case E_FUNC_CALL: gen_func_call_expr(self, expr); break;
    case E_INTEGER: gen_integer_expr(self, expr); break;
    case E_VARIABLE_REF: gen_variable_ref(self, expr); break;
    case E_STRING: gen_string_expr(self, expr); break;
    case E_BLOCK: gen_block_expr(self, expr); break;
    }
}

static void gen_function_param(CodeGenerator* self, Stmt* param, u64 idx) {
    /* offset is used as an index into register table for parameters */
    param->variable_decl.offset = idx;
    if (idx > 5) {
        u64 type_sz = get_data_type_size(self, param->variable_decl.data_type);
        param->variable_decl.offset =
            self->next_param_stack_offset +
            type_sz +
            /* 8 = sizeof(ret_addr) */
            8;
        self->next_param_stack_offset += type_sz;
    }
}

static void set_var_offsets_from_rbp(CodeGenerator* self, Stmt** vars) {
    static u64 type_sz_idx_tbl[4] = {
        DT_SIZE_64,
        DT_SIZE_32,
        DT_SIZE_16,
        DT_SIZE_8
    };

    for (u8 i = 0; i < 4; i++) {
        buf_loop(vars, v) {
            u64 sz = get_data_type_size(self, vars[v]->variable_decl.data_type);
            if (sz == type_sz_idx_tbl[i]) {
                vars[v]->variable_decl.offset = self->next_local_var_offset + sz;
                self->next_local_var_offset += sz;
            }
        }
    }
}

static void gen_function(CodeGenerator* self, Stmt* stmt) {
    if (stmt->function.decl) return;
    set_var_offsets_from_rbp(self, stmt->function.variable_decls);

    self->enclosed_function = stmt;
    if (stmt->function.pub) {
        asmp(self, "global %s", stmt->function.identifier->lexeme);
    }
    label_from_token(self, stmt->function.identifier);
    asmw(self, "push rbp");
    asmw(self, "mov rbp, rsp");
    asmw(self, "push rbx");

    buf_loop(stmt->function.params, p) {
        gen_function_param(self, stmt->function.params[p], p);
    }

    u64 resv_local_variables_stack_space = 0;
    Stmt** variable_decls = stmt->function.variable_decls;
    buf_loop(variable_decls, v) {
        if (variable_decls[v]->variable_decl.param) continue;
        resv_local_variables_stack_space +=
            get_data_type_size(
                    self,
                    variable_decls[v]->variable_decl.data_type
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

    gen_expr(self, stmt->function.block);
    self->enclosed_function = null;

    label(self, ".return");
    asmw(self, "pop rbx");
    asmw(self, "leave");
    asmw(self, "ret");
    asmw(self, "");
}

static void gen_variable_decl(CodeGenerator* self, Stmt* stmt) {
    if (stmt->variable_decl.initializer) {
        gen_expr(self, stmt->variable_decl.initializer);
        move_rax_into_variable(self, stmt);
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

static void gen_rodata(CodeGenerator* self) {
    asmw(self, "section .rodata");
    buf_loop(self->static_strings, s) {
        label(self, self->static_strings[s]->a);
        gen_tab(self);
        gen_str(self, "db `");
        gen_str(self, self->static_strings[s]->b);
        gen_str(self, "\\0`");
        gen_newline(self);
    }
}

void gen_code_for_ast(CodeGenerator* self, Ast* ast, const char* fpath) {
    self->ast = ast;
    self->enclosed_function = null;
    self->code = null;
    self->indent = 0;
    self->next_local_var_offset = 0;
    self->next_param_stack_offset = 0;
    self->static_strings = null;
    self->next_free_string_label = 0;

    gen_prelude(self);
    buf_loop(self->ast->func_sym_tbl, s) {
        gen_stmt(self, self->ast->stmts[s]);
    }
    gen_rodata(self);
    buf_push(self->code, '\0');

    FILE* file = fopen(fpath, "w");
    fwrite(self->code, sizeof(char), buf_len(self->code) - 1, file);
    fclose(file);
    printf("%s", self->code);
}
