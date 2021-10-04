#include "code_gen_linux-x64-nasm.h"
#include "buf.h"
#include "stri.h"
#include "msg.h"

typedef void (*AsmpFunc)(CodeGenContext*, char*, ...);

typedef enum {
    WHILE_LOOP_ASM_COND,
    WHILE_LOOP_ASM_END,
} WhileLoopAsmLabel;

static void code_gen_stmt(CodeGenContext* c, Stmt* stmt);
static void code_gen_function_stmt(CodeGenContext* c, Stmt* stmt);
static void code_gen_variable_stmt(CodeGenContext* c, Stmt* stmt);
static void code_gen_assign_stmt(CodeGenContext* c, Stmt* stmt);
static void code_gen_expr_stmt(CodeGenContext* c, Stmt* stmt);
static void code_gen_expr(CodeGenContext* c, Expr* expr);
static void code_gen_integer_expr(CodeGenContext* c, Expr* expr);
static void code_gen_constant_expr(CodeGenContext* c, Expr* expr);
static void code_gen_symbol_expr(CodeGenContext* c, Expr* expr);
static void code_gen_function_call_expr(CodeGenContext* c, Expr* expr);
static void code_gen_binop_expr(CodeGenContext* c, Expr* expr);
static void code_gen_block_expr(CodeGenContext* c, Expr* expr);
static void code_gen_if_expr(CodeGenContext* c, Expr* expr);
static void code_gen_if_branch(
        CodeGenContext* c, 
        IfBranch* br, 
        IfBranchKind nextbr_kind,
        size_t idx,
        size_t elseifidx);
static void code_gen_distinct_if_label(
        CodeGenContext* c, 
        IfBranchKind kind, 
        size_t idx,
        size_t elseifidx,
        bool is_definition);
static void code_gen_while_expr(CodeGenContext* c, Expr* expr);
static void code_gen_distinct_while_label(
        CodeGenContext* c,
        WhileLoopAsmLabel whilelb_kind,
        size_t idx,
        bool is_definition);
static AsmpFunc code_gen_get_asmp_func(bool is_definition);
static void code_gen_zs_extend(CodeGenContext* c, Type* from, Type* to);
static void code_gen_assign_reg_group_to_stack_addr(
        CodeGenContext* c, 
        Stmt* stmt, 
        RegisterGroupKind kind);
static void code_gen_assign_stack_addr_to_reg_group(
        CodeGenContext* c, 
        Stmt* stmt, 
        RegisterGroupKind kind);
static void code_gen_stack_addr(CodeGenContext* c, Stmt* stmt);
static size_t code_gen_get_sizeof_symbol_on_stack(Stmt* stmt);
static char* code_gen_get_asm_type_specifier(size_t bytes);
static char* code_gen_get_rax_register_by_size(size_t bytes);
static char* code_gen_get_rcx_register_by_size(size_t bytes);
static char* code_gen_get_arg_register_by_idx(size_t idx);
static void code_gen_asmp(CodeGenContext* c, char* fmt, ...);
static void code_gen_tasmp(CodeGenContext* c, char* fmt, ...);
static void code_gen_ntasmp(CodeGenContext* c, char* fmt, ...);
static void code_gen_asmw(CodeGenContext* c, char* str);
static void code_gen_nasmw(CodeGenContext* c, char* str);
static void code_gen_asmplb(CodeGenContext* c, char* labelfmt, ...);
static void code_gen_asmwlb(CodeGenContext* c, char* label);
static void code_gen_push_tabs(CodeGenContext* c);

void code_gen(CodeGenContext* c) {
    c->asm_code = null;

    buf_loop(c->srcfile->stmts, i) {
        code_gen_stmt(c, c->srcfile->stmts[i]);
    }
    buf_push(c->asm_code, '\0');
    /* fputs(c->asm_code, stdout); */

    FILE* file = fopen("build/tmp.asm", "w");
    fwrite(c->asm_code, buf_len(c->asm_code)-1, sizeof(char), file);
    fclose(file);
    system("nasm -felf64 build/tmp.asm");
    system("gcc -g -no-pie -o build/tmp build/tmp.o examples/std.c");
}

void code_gen_stmt(CodeGenContext* c, Stmt* stmt) {
    switch (stmt->kind) {
        case STMT_KIND_FUNCTION: {
            code_gen_function_stmt(c, stmt);
        } break;

        case STMT_KIND_VARIABLE: {
            code_gen_variable_stmt(c, stmt);
        } break;

        case STMT_KIND_ASSIGN: {
            code_gen_assign_stmt(c, stmt);
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
        code_gen_asmwlb(c, stmt->function.header->identifier->lexeme);
        code_gen_asmw(c, "push rbp");
        code_gen_asmw(c, "mov rbp, rsp");

        Stmt** params = stmt->function.header->params;
        for (size_t i = 0; i < MIN(buf_len(params), 6); i++) {
            stmt->function.stack_vars_size += PTR_SIZE_BYTES;
            stmt->function.stack_vars_size = 
                align_to_pow2(
                        stmt->function.stack_vars_size, 
                        PTR_SIZE_BYTES);
            params[i]->param.stack_offset = 
                stmt->function.stack_vars_size;
        }

        size_t stack_vars_size_align16 = 0;
        if (stmt->function.stack_vars_size != 0) {
            stack_vars_size_align16 = 
                align_to_pow2(stmt->function.stack_vars_size, 16);
            code_gen_asmp(c, "sub rsp, %lu", stack_vars_size_align16);
        }
        
        buf_loop(params, i) {
            if (i < 6) {
                code_gen_assign_reg_group_to_stack_addr(
                        c,
                        params[i],
                        REGISTER_ARG);
            }
        }

        code_gen_block_expr(c, stmt->function.body);
        /* code_gen_zs_extend( */
        /*         c, */ 
        /*         stmt->function.body->type, */ 
        /*         stmt->function.header->return_type); */

        code_gen_asmwlb(c, ".Lret");
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
        /* code_gen_zs_extend(c, stmt->variable.initializer_type, stmt->variable.type); */
        code_gen_assign_reg_group_to_stack_addr(
                c,
                stmt,
                REGISTER_ACC);
    }
}

void code_gen_assign_stmt(CodeGenContext* c, Stmt* stmt) {
    code_gen_asmw(c, "");
    code_gen_expr(c, stmt->assign.right);
    code_gen_assign_reg_group_to_stack_addr(
            c, 
            stmt->assign.left->symbol.ref,
            REGISTER_ACC);
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

        case EXPR_KIND_BINOP: {
            code_gen_binop_expr(c, expr);
        } break;

        case EXPR_KIND_BLOCK: {
            code_gen_block_expr(c, expr);
        } break;

        case EXPR_KIND_IF: {
            code_gen_if_expr(c, expr);
        } break;

        case EXPR_KIND_WHILE: {
            code_gen_while_expr(c, expr);
        } break;
    }
}

void code_gen_integer_expr(CodeGenContext* c, Expr* expr) {
    code_gen_asmp(
            c, 
            "mov rax, %s", 
            expr->integer.integer->lexeme);
}

void code_gen_constant_expr(CodeGenContext* c, Expr* expr) {
    int value = 0;
    switch (expr->constant.kind) {
        case CONSTANT_KIND_BOOLEAN_TRUE: {
            value = 1;
        } break;
    }

    code_gen_asmp(
            c, 
            "mov eax, %d",
            value);
}

void code_gen_symbol_expr(CodeGenContext* c, Expr* expr) {
    Stmt* ref = expr->symbol.ref;
    switch (ref->kind) { 
        case STMT_KIND_VARIABLE: {
            if (ref->parent_func) {
                // TODO: cache `bytes`
                code_gen_assign_stack_addr_to_reg_group(
                        c,
                        ref,
                        REGISTER_ACC);
            }
            else assert(0);
            code_gen_zs_extend(c, ref->variable.type, expr->type);
        } break;

        case STMT_KIND_PARAM: {
            code_gen_assign_stack_addr_to_reg_group(
                    c,
                    ref,
                    REGISTER_ACC);
            code_gen_zs_extend(c, ref->param.type, expr->type);
        } break;

        case STMT_KIND_FUNCTION: {
            assert(0);
        } break;
    }
}

void code_gen_function_call_expr(CodeGenContext* c, Expr* expr) {
    Expr** args = expr->function_call.args;
    Stmt** params = expr->function_call.callee->symbol.ref->function.header->params;
    size_t const args_len = buf_len(args);
    size_t reg_args_len = MIN(args_len, 6);
    for (size_t i = 0; i < reg_args_len; i++) {
        code_gen_expr(c, args[i]);
        code_gen_zs_extend(c, args[i]->type, params[i]->param.type);
        code_gen_asmp(
                c,
                "mov %s, rax",
                code_gen_get_arg_register_by_idx(i));
    }
    if (reg_args_len < args_len) {
        for (size_t i = args_len-1; i >= reg_args_len; i--) {
            code_gen_expr(c, args[i]);
            code_gen_zs_extend(c, args[i]->type, params[i]->param.type);
            code_gen_asmw(c, "push rax");
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

void code_gen_binop_expr(CodeGenContext* c, Expr* expr) {
    if ((expr->binop.op->kind == TOKEN_KIND_PLUS ||
        expr->binop.op->kind == TOKEN_KIND_MINUS) &&
        (type_is_apint(expr->binop.left_type) && 
         type_is_apint(expr->binop.right_type))) {
        code_gen_asmp(
                c, 
                "mov rax, %lu ; simplified", 
                bigint_get_lsd(expr->type->builtin.apint));
    }
    else {
        Type* bigger_type = null;
        size_t const left_bytes = type_bytes(expr->binop.left_type);
        size_t const right_bytes = type_bytes(expr->binop.right_type);
        if (expr->type) {
            bigger_type = expr->type;
        }
        else if (left_bytes > right_bytes) {
            bigger_type = expr->binop.left_type;
        }
        else {
            bigger_type = expr->binop.right_type;
        }
        size_t const bigger_bytes = type_bytes(bigger_type);

        code_gen_expr(c, expr->binop.left);
        if (!type_is_apint(expr->binop.left_type)) {
            code_gen_zs_extend(c, expr->binop.left_type, bigger_type);
        }
        code_gen_asmw(c, "push rax");

        code_gen_expr(c, expr->binop.right);
        if (!type_is_apint(expr->binop.right_type)) {
            code_gen_zs_extend(c, expr->binop.right_type, bigger_type);
        }
        code_gen_asmw(c, "pop rcx");
        code_gen_asmw(c, "xchg rax, rcx");

        if (expr->binop.op->kind == TOKEN_KIND_PLUS ||
            expr->binop.op->kind == TOKEN_KIND_MINUS) {
            code_gen_asmp(
                    c, 
                    "%s %s, %s",
                    (expr->binop.op->kind == TOKEN_KIND_PLUS ? "add" : "sub"),
                    code_gen_get_rax_register_by_size(bigger_bytes),
                    code_gen_get_rcx_register_by_size(bigger_bytes));
        }
        else if (token_is_cmp_op(expr->binop.op)) {
            code_gen_asmp(
                    c, 
                    "cmp %s, %s",
                    code_gen_get_rax_register_by_size(bigger_bytes),
                    code_gen_get_rcx_register_by_size(bigger_bytes));
            code_gen_asmw(c, "mov eax, 0");
            
            char* inst = null; 
            switch (expr->binop.op->kind) {
                case TOKEN_KIND_DOUBLE_EQUAL: inst = "sete"; break;
                case TOKEN_KIND_BANG_EQUAL: inst = "setne"; break;
                case TOKEN_KIND_LANGBR: inst = "setl"; break;
                case TOKEN_KIND_LANGBR_EQUAL: inst = "setle"; break;
                case TOKEN_KIND_RANGBR: inst = "setg"; break;
                case TOKEN_KIND_RANGBR_EQUAL: inst = "setge"; break;
                default: assert(0); break;
            }
            code_gen_asmp(c, "%s al", inst);
        }
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
    /* code_gen_asmw(c, "; -- if --"); */
    code_gen_asmw(c, "");
    size_t idx = expr->parent_func->function.ifidx++;
    IfBranchKind ifbr_next;
    if (expr->iff.elseifbr) {
        ifbr_next = IF_BRANCH_ELSEIF;
    }
    else if (expr->iff.elsebr) {
        ifbr_next = IF_BRANCH_ELSE;
    } 
    else {
        ifbr_next = IF_BRANCH_IF;
    }
    code_gen_if_branch(c, expr->iff.ifbr, ifbr_next, idx, 0);

    buf_loop(expr->iff.elseifbr, i) {
        IfBranchKind elseifbr_next;
        if (i < buf_len(expr->iff.elseifbr)-1) {
            elseifbr_next = IF_BRANCH_ELSEIF;
        }
        else if (expr->iff.elsebr) {
            elseifbr_next = IF_BRANCH_ELSE;
        }
        else {
            elseifbr_next = IF_BRANCH_IF;
        }
        code_gen_if_branch(
                c, 
                expr->iff.elseifbr[i], 
                elseifbr_next, 
                idx,
                i);
    }

    if (expr->iff.elsebr) {
        code_gen_if_branch(c, expr->iff.elsebr, IF_BRANCH_IF, idx, 0);
    }

    code_gen_distinct_if_label(c, IF_BRANCH_IF, idx, 0, true);
}

void code_gen_if_branch(
        CodeGenContext* c, 
        IfBranch* br, 
        IfBranchKind nextbr_kind,
        size_t idx,
        size_t elseifidx) {
    if (br->kind != IF_BRANCH_IF) {
        code_gen_distinct_if_label(c, br->kind, idx, elseifidx, true);
    }

    if (br->kind != IF_BRANCH_ELSE) {
        code_gen_expr(c, br->cond);
        code_gen_asmw(c, "test rax, rax");
        code_gen_nasmw(c, "jz ");
        if (br->kind == IF_BRANCH_ELSEIF) elseifidx++;
        code_gen_distinct_if_label(c, nextbr_kind, idx, elseifidx, false);
    }

    code_gen_block_expr(c, br->body);
    if (br->kind != IF_BRANCH_ELSE) {
        code_gen_nasmw(c, "jmp ");
        code_gen_distinct_if_label(c, IF_BRANCH_IF, idx, 0, false);
    }
}

void code_gen_distinct_if_label(
        CodeGenContext* c, 
        IfBranchKind kind, 
        size_t idx,
        size_t elseifidx,
        bool is_definition) {
    char* fmt = null;
    switch (kind) {
        case IF_BRANCH_IF: fmt = ".Lendif%lu"; break;
        case IF_BRANCH_ELSEIF: fmt = ".Lelseif%lu_%lu"; break;
        case IF_BRANCH_ELSE: fmt = ".Lelse%lu"; break;
    }

    AsmpFunc asmp_func = code_gen_get_asmp_func(is_definition);
    if (kind == IF_BRANCH_ELSEIF) {
        asmp_func(c, fmt, idx, elseifidx);
    }
    else {
        asmp_func(c, fmt, idx);
    }
}

void code_gen_while_expr(CodeGenContext* c, Expr* expr) {
    code_gen_asmw(c, "");
    size_t idx = expr->parent_func->function.whileidx++;
    code_gen_distinct_while_label(c, WHILE_LOOP_ASM_COND, idx, true);

    code_gen_expr(c, expr->whilelp.cond);
    code_gen_asmw(c, "test rax, rax");
    code_gen_nasmw(c, "jz ");
    code_gen_distinct_while_label(c, WHILE_LOOP_ASM_END, idx, false);

    code_gen_block_expr(c, expr->whilelp.body);
    code_gen_nasmw(c, "jmp ");
    code_gen_distinct_while_label(c, WHILE_LOOP_ASM_COND, idx, false);
    code_gen_distinct_while_label(c, WHILE_LOOP_ASM_END, idx, true);
}

void code_gen_distinct_while_label(
        CodeGenContext* c,
        WhileLoopAsmLabel whilelb_kind,
        size_t idx,
        bool is_definition) {
    char* fmt = null;
    switch (whilelb_kind) {
        case WHILE_LOOP_ASM_COND: fmt = ".Lwhilecond%lu"; break;
        case WHILE_LOOP_ASM_END: fmt = ".Lendwhile%lu"; break;
    }

    AsmpFunc asmp_func = code_gen_get_asmp_func(is_definition);
    asmp_func(c, fmt, idx);
}

AsmpFunc code_gen_get_asmp_func(bool is_definition) {
    if (is_definition) {
        return code_gen_asmplb;
    } 
    else {
        return code_gen_tasmp;
    }
}

void code_gen_zs_extend(CodeGenContext* c, Type* from, Type* to) {
    if (!from || !to) return;
    if (from->kind == TYPE_KIND_BUILTIN && to->kind == TYPE_KIND_BUILTIN) {
        bool is_from_apint = type_is_apint(from);
        bool is_to_apint = type_is_apint(to);
        if (is_from_apint && is_to_apint) {
            assert(0);
        }
        else if ((type_is_integer(from) && type_is_integer(to)) ||
                 (is_from_apint || is_to_apint)) {
            Type* int_type = from;
            Type* apint_type = to;
            if (is_from_apint) { 
                SWAP_VARS(Type*, int_type, apint_type);
            }

            bool is_signed = builtin_type_is_signed(int_type->builtin.kind);
            size_t from_bytes = type_bytes(from);
            size_t to_bytes = type_bytes(to);
            if (from_bytes == to_bytes ||
                (!is_signed && from_bytes == 4 && to_bytes == 8)) {
                return;
            }
            else {
                char* fmt = null;
                // TODO: should the dest be fixed (8 bytes) or variable?
                if (is_signed) fmt = "movsx %s, %s";
                else fmt = "movzx %s, %s";
                code_gen_asmp(
                        c,
                        fmt,
                        code_gen_get_rax_register_by_size(to_bytes),
                        code_gen_get_rax_register_by_size(from_bytes));
            }
        }
        else {
            assert(0);
        }
    }
}

void code_gen_assign_reg_group_to_stack_addr(
        CodeGenContext* c, 
        Stmt* stmt, 
        RegisterGroupKind kind) {
    code_gen_nasmw(c, "mov ");
    code_gen_stack_addr(c, stmt);

    char* reg = null;
    switch (kind) {
        case REGISTER_ACC:
            reg = code_gen_get_rax_register_by_size(
                    code_gen_get_sizeof_symbol_on_stack(stmt));
            break;
        case REGISTER_ARG:
            reg = code_gen_get_arg_register_by_idx(stmt->param.idx);
            break;
    }
    code_gen_tasmp(c, ", %s", reg);
}

void code_gen_assign_stack_addr_to_reg_group(
        CodeGenContext* c, 
        Stmt* stmt, 
        RegisterGroupKind kind) {
    code_gen_nasmw(c, "mov ");
    char* reg = null;
    switch (kind) {
        case REGISTER_ACC:
            reg = code_gen_get_rax_register_by_size(
                    code_gen_get_sizeof_symbol_on_stack(stmt));
            break;
        case REGISTER_ARG:
            reg = code_gen_get_arg_register_by_idx(stmt->param.idx);
            break;
    }
    code_gen_ntasmp(c, "%s, ", reg);

    code_gen_stack_addr(c, stmt);
    code_gen_asmw(c, "");
}

void code_gen_stack_addr(CodeGenContext* c, Stmt* stmt) {
    switch (stmt->kind) {
        case STMT_KIND_VARIABLE: {
            code_gen_ntasmp(
                    c,
                    "%s [rbp - %lu]",
                    code_gen_get_asm_type_specifier(
                        type_bytes(stmt->variable.type)),
                    stmt->variable.stack_offset);
        } break;

        case STMT_KIND_PARAM: {
            if (stmt->param.idx < 6) {
                code_gen_ntasmp(
                        c, 
                        "qword [rbp - %lu]",
                        stmt->param.stack_offset);
            }
            else {
                code_gen_ntasmp(
                        c, 
                        "qword [rbp + %lu]",
                        16 + ((stmt->param.idx-6) * PTR_SIZE_BYTES));
            }
        } break;
    }
}

size_t code_gen_get_sizeof_symbol_on_stack(Stmt* stmt) {
    switch (stmt->kind) {
        case STMT_KIND_VARIABLE: 
            return type_bytes(stmt->variable.type); 
        case STMT_KIND_PARAM:
            return PTR_SIZE_BYTES;
    }
    assert(0);
    return 0;
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

char* code_gen_get_rcx_register_by_size(size_t bytes) {
    switch (bytes) {
        case 1: return "cl";
        case 2: return "cx";
        case 4: return "ecx";
        case 8: return "rcx";
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

// No tabs at beginning
void code_gen_tasmp(CodeGenContext* c, char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    buf_vprintf(c->asm_code, fmt, ap);
    buf_push(c->asm_code, '\n');
    va_end(ap);
}

// No tabs at beginning, no newline at end
void code_gen_ntasmp(CodeGenContext* c, char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    buf_vprintf(c->asm_code, fmt, ap);
    va_end(ap);
}

void code_gen_asmw(CodeGenContext* c, char* str) {
    code_gen_push_tabs(c);
    buf_write(c->asm_code, str);
    buf_push(c->asm_code, '\n');
}

// No newline at end
void code_gen_nasmw(CodeGenContext* c, char* str) {
    code_gen_push_tabs(c);
    buf_write(c->asm_code, str);
}

void code_gen_asmplb(CodeGenContext* c, char* labelfmt, ...) {
    va_list ap;
    va_start(ap, labelfmt);
    buf_vprintf(c->asm_code, labelfmt, ap);
    buf_write(c->asm_code, ":\n");
    va_end(ap);
}

void code_gen_asmwlb(CodeGenContext* c, char* label) {
    buf_printf(c->asm_code, "%s:\n", label);
}

void code_gen_push_tabs(CodeGenContext* c) {
    for (int i = 0; i < TAB_SIZE; i++) {
        buf_push(c->asm_code, ' ');
    }
}
