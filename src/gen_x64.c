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

static void gen_prelude(CodeGenerator* self) {
    asmw(self, "section .text");
}

static void gen_stmt(CodeGenerator* self, Stmt* stmt);

static void gen_function(CodeGenerator* self, Stmt* stmt) {
    label_from_token(self, stmt->function.identifier);
    asmw(self, "push rbp");
    asmw(self, "mov rbp, rsp");

    u64 resv_local_variables_stack_space = 0;
    buf_loop(stmt->function.variable_decls, v) {
        resv_local_variables_stack_space +=
            get_data_type_size(
                    self,
                    stmt->function.variable_decls[v]->variable_decl.data_type
            );
    }
    printf("stack space: %lu\n", resv_local_variables_stack_space);

    buf_loop(stmt->function.block->block.stmts, s) {
        gen_stmt(self, stmt->function.block->block.stmts[s]);
    }

    asmw(self, "leave");
    asmw(self, "ret");
}

static void gen_stmt(CodeGenerator* self, Stmt* stmt) {
    switch (stmt->type) {
    case S_FUNCTION: gen_function(self, stmt); break;
    }
}

void gen_code_for_ast(CodeGenerator* self, Ast* ast, const char* fpath) {
    self->ast = ast;
    self->code = null;

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
