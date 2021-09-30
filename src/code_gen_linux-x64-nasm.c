#include "code_gen.h"
#include "buf.h"
#include "stri.h"
#include "msg.h"

static void code_gen_stmt(CodeGenContext* c, Stmt* stmt);
static void code_gen_function_stmt(CodeGenContext* c, Stmt* stmt);
static void code_gen_variable_stmt(CodeGenContext* c, Stmt* stmt);
static void code_gen_expr_stmt(CodeGenContext* c, Stmt* stmt);
static void code_gen_expr(CodeGenContext* c, Expr* expr);
static void code_gen_integer_expr(CodeGenContext* c, Expr* expr);
static void code_gen_constant_expr(CodeGenContext* c, Expr* expr);
static void code_gen_symbol_expr(CodeGenContext* c, Expr* expr);
static void code_gen_function_call_expr(CodeGenContext* c, Expr* expr);
static void code_gen_block_expr(CodeGenContext* c, Expr* expr);
static void code_gen_if_expr(CodeGenContext* c, Expr* expr);
static void code_gen_if_branch(CodeGenContext* c, IfBranch* br);
static char* code_gen_get_asm_type_specifier(size_t bytes);
static char* code_gen_get_rax_register_by_size(size_t bytes);
static char* code_gen_get_arg_register_by_idx(size_t idx);
static void code_gen_asmp(CodeGenContext* c, char* fmt, ...);
static void code_gen_asmw(CodeGenContext* c, char* str);
static void code_gen_asmlb(CodeGenContext* c, char* label);
static void code_gen_push_tabs(CodeGenContext* c);

void code_gen(CodeGenContext* c) {
    c->asm_code = null;

    buf_loop(c->srcfile->stmts, i) {
        code_gen_stmt(c, c->srcfile->stmts[i]);
    }
    buf_push(c->asm_code, '\0');
    fputs(c->asm_code, stdout);

    FILE* file = fopen("build/tmp_asm.asm", "w");
    fwrite(c->asm_code, buf_len(c->asm_code)-1, sizeof(char), file);
    fclose(file);
    system("nasm -felf64 build/tmp_asm.asm");
    system("gcc -g -no-pie -o build/tmp build/tmp_asm.o examples/std.c");
}

void code_gen_stmt(CodeGenContext* c, Stmt* stmt) {
    switch (stmt->kind) {
        case STMT_KIND_FUNCTION: {
            code_gen_function_stmt(c, stmt);
        } break;

        case STMT_KIND_VARIABLE: {
            code_gen_variable_stmt(c, stmt);
        } break;

        case STMT_KIND_EXPR: {
            code_gen_expr_stmt(c, stmt);
        } break;
    }
}

void code_gen_function_stmt(CodeGenContext* c, Stmt* stmt) {
    if (stmt->function.is_extern) {
        code_gen_asmp(c, "extern %s", stmt->function.header->identifier->lexeme);
    } 
    else {
        code_gen_asmp(c, "global %s", stmt->function.header->identifier->lexeme);
        code_gen_asmlb(c, stmt->function.header->identifier->lexeme);
        code_gen_asmw(c, "push rbp");
        code_gen_asmw(c, "mov rbp, rsp");

        Stmt** params = stmt->function.header->params;
        for (size_t i = 0; i < MIN(buf_len(params), 6); i++) {
            stmt->function.stack_vars_size += PTR_SIZE_BYTES;
            stmt->function.stack_vars_size = 
                round_to_next_multiple(
                        stmt->function.stack_vars_size, 
                        PTR_SIZE_BYTES);
            params[i]->param.stack_offset = 
                stmt->function.stack_vars_size;
        }

        size_t stack_vars_size_align16 = 0;
        if (stmt->function.stack_vars_size != 0) {
            stack_vars_size_align16 = 
                round_to_next_multiple(stmt->function.stack_vars_size, 16);
            code_gen_asmp(c, "sub rsp, %lu", stack_vars_size_align16);
        }
        
        buf_loop(params, i) {
            if (i < 6) {
                code_gen_asmp(
                        c, 
                        "mov qword [rbp - %lu], %s",
                        params[i]->param.stack_offset, 
                        code_gen_get_arg_register_by_idx(i));
            }
        }

        code_gen_block_expr(c, stmt->function.body);

        code_gen_asmlb(c, ".Lret");
        if (stmt->function.stack_vars_size != 0) {
            code_gen_asmp(c, "add rsp, %lu", stack_vars_size_align16);
        }
        code_gen_asmw(c, "pop rbp");
        code_gen_asmw(c, "ret");
        code_gen_asmw(c, "");
    }
}

void code_gen_variable_stmt(CodeGenContext* c, Stmt* stmt) {
    size_t bytes = type_bytes(stmt->variable.type);
    if (stmt->variable.initializer && bytes != 0) {
        code_gen_expr(c, stmt->variable.initializer);
        code_gen_asmp(
                c,
                "mov %s [rbp - %lu], %s",
                code_gen_get_asm_type_specifier(bytes),
                stmt->variable.stack_offset,
                code_gen_get_rax_register_by_size(bytes));
    }
}

void code_gen_expr_stmt(CodeGenContext* c, Stmt* stmt) {
    code_gen_expr(c, stmt->expr.child);
}

void code_gen_expr(CodeGenContext* c, Expr* expr) {
    switch (expr->kind) {
        case EXPR_KIND_INTEGER: {
            code_gen_integer_expr(c, expr);
        } break;

        case EXPR_KIND_CONSTANT: {
            code_gen_constant_expr(c, expr);
        } break;

        case EXPR_KIND_SYMBOL: {
            code_gen_symbol_expr(c, expr);
        } break;

        case EXPR_KIND_FUNCTION_CALL: {
            code_gen_function_call_expr(c, expr);
        } break;

        case EXPR_KIND_BLOCK: {
            code_gen_block_expr(c, expr);
        } break;

        case EXPR_KIND_IF: {
            code_gen_if_expr(c, expr);
        } break;
    }
}

void code_gen_integer_expr(CodeGenContext* c, Expr* expr) {
    code_gen_asmp(c, "mov rax, %s", expr->integer.integer->lexeme);
}

void code_gen_constant_expr(CodeGenContext* c, Expr* expr) {
    switch (expr->constant.kind) {
        case CONSTANT_KIND_BOOLEAN_TRUE: {
            code_gen_asmp(c, "mov rax, 1");
        } break;

        case CONSTANT_KIND_BOOLEAN_FALSE: {
            code_gen_asmp(c, "mov rax, 0");
        } break;

        case CONSTANT_KIND_NULL: {
            code_gen_asmp(c, "mov rax, 0");
        } break;
    }
}

void code_gen_symbol_expr(CodeGenContext* c, Expr* expr) {
    Stmt* ref = expr->symbol.ref;
    switch (ref->kind) { 
        case STMT_KIND_VARIABLE: {
            if (ref->variable.parent_func) {
                // TODO: cache `bytes`
                size_t bytes = type_bytes(ref->variable.type);
                code_gen_asmp(
                        c,
                        "mov %s, %s [rbp - %lu]",
                        code_gen_get_rax_register_by_size(bytes),
                        code_gen_get_asm_type_specifier(bytes),
                        ref->variable.stack_offset);
            }
            else assert(0);
        } break;

        case STMT_KIND_PARAM: {
            if (ref->param.idx < 6) {
                code_gen_asmp(
                        c,
                        "mov rax, qword [rbp - %lu]",
                        ref->param.stack_offset);
            }
            else {
                code_gen_asmp(
                        c, 
                        "mov rax, qword [rbp + %lu]",
                        16 + ((ref->param.idx-6) * PTR_SIZE_BYTES));
            }
        } break;

        case STMT_KIND_FUNCTION: {
            assert(0);
        } break;
    }
}

void code_gen_function_call_expr(CodeGenContext* c, Expr* expr) {
    Expr** args = expr->function_call.args;
    size_t const args_len = buf_len(args);
    size_t reg_args_len = MIN(args_len, 6);
    for (size_t i = 0; i < reg_args_len; i++) {
        code_gen_expr(c, args[i]);
        code_gen_asmp(
                c,
                "mov %s, rax",
                code_gen_get_arg_register_by_idx(i));
    }
    if (reg_args_len < args_len) {
        for (size_t i = args_len-1; i >= reg_args_len; i--) {
            code_gen_expr(c, args[i]);
            code_gen_asmp(c, "push rax");
        }
    }
    code_gen_asmp(
            c, 
            "call %s", 
            expr->function_call.callee->symbol.identifier->lexeme);
    if (reg_args_len < args_len) {
        code_gen_asmp(
                c,
                "add rsp, %lu",
                (args_len - reg_args_len) * PTR_SIZE_BYTES);
    }
}

void code_gen_block_expr(CodeGenContext* c, Expr* expr) {
    buf_loop(expr->block.stmts, i) {
        code_gen_stmt(c, expr->block.stmts[i]);
    }
    if (expr->block.value) {
        code_gen_expr(c, expr->block.value);
    }
}

void code_gen_if_expr(CodeGenContext* c, Expr* expr) {
    code_gen_asmw(c, "; -- if --");
    code_gen_if_branch(c, expr->iff.ifbr);
    code_gen_if_branch(c, expr->iff.elsebr);
    code_gen_asmlb(c, ".Lendif");
}

void code_gen_if_branch(CodeGenContext* c, IfBranch* br) {
    if (br->kind == IF_BRANCH_ELSE) {
        code_gen_asmlb(c, ".Lelse");
    }
    if (br->kind != IF_BRANCH_ELSE) {
        code_gen_expr(c, br->cond);
        code_gen_asmw(c, "test rax, rax");
        code_gen_asmp(c, "jz .Lelse");
    }
    code_gen_block_expr(c, br->body);
    if (br->kind != IF_BRANCH_ELSE) {
        code_gen_asmp(c, "jmp .Lendif");
    }
}

char* code_gen_get_asm_type_specifier(size_t bytes) {
    switch (bytes) {
        case 1: return "BYTE";
        case 2: return "WORD";
        case 4: return "DWORD";
        case 8: return "QWORD";
    }
    assert(0);
    return null;
}

char* code_gen_get_rax_register_by_size(size_t bytes) {
    switch (bytes) {
        case 1: return "al";
        case 2: return "ax";
        case 4: return "eax";
        case 8: return "rax";
    }
    assert(0);
    return null;
}

char* code_gen_get_arg_register_by_idx(size_t idx) {
    switch (idx) {
        case 0: return "rdi";
        case 1: return "rsi";
        case 2: return "rdx";
        case 3: return "rcx";
        case 4: return "r8";
        case 5: return "r9";
    }
    assert(0);
    return null;
}

void code_gen_asmp(CodeGenContext* c, char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    code_gen_push_tabs(c);
    buf_vprintf(c->asm_code, fmt, ap);
    buf_push(c->asm_code, '\n');
    va_end(ap);
}

void code_gen_asmw(CodeGenContext* c, char* str) {
    code_gen_push_tabs(c);
    buf_write(c->asm_code, str);
    buf_push(c->asm_code, '\n');
}

void code_gen_asmlb(CodeGenContext* c, char* label) {
    buf_printf(c->asm_code, "%s:\n", label);
}

void code_gen_push_tabs(CodeGenContext* c) {
    for (int i = 0; i < TAB_SIZE; i++) {
        buf_push(c->asm_code, ' ');
    }
}
