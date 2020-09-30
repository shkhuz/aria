#include "arpch.h"
#include "util/util.h"
#include "error_msg.h"
#include "ds/ds.h"
#include "aria.h"

#define gen_l_paren(self) gen_chr(self, '(')
#define gen_r_paren(self) gen_chr(self, ')')
#define gen_l_brace(self) gen_chr(self, '{')
#define gen_r_brace(self) gen_chr(self, '}')
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

static void gen_line(CodeGenerator* self, char* str) {
    gen_str(self, str);
    gen_newline(self);
}

static void gen_token(CodeGenerator* self, Token* token) {
    gen_str(self, token->lexeme);
}

static void gen_data_type(CodeGenerator* self, DataType* dt) {
    gen_token(self, dt->identifier);
    for (u64 p = 0; p < dt->pointer_count; p++) {
        gen_chr(self, '*');
    }
}

static void gen_function_header(CodeGenerator* self, Stmt* stmt) {
    assert(stmt->type == S_FUNCTION);
    gen_data_type(self, stmt->function.return_type);
    gen_space(self);
    gen_token(self, stmt->function.identifier);
    gen_l_paren(self);
    gen_r_paren(self);
}

static void gen_function_decls(CodeGenerator* self) {
    buf_loop(self->ast->stmts, s) {
        Stmt* stmt = self->ast->stmts[s];
        if (stmt->type == S_FUNCTION) {
            gen_function_header(self, stmt);
            gen_semicolon(self);
            gen_newline(self);
        }
    }
}

static void gen_functions(CodeGenerator* self) {
    buf_loop(self->ast->stmts, s) {
        Stmt* stmt = self->ast->stmts[s];
        if (stmt->type == S_FUNCTION) {
            gen_function_header(self, stmt);
            gen_l_brace(self);
            gen_r_brace(self);
            gen_newline(self);
        }
    }
}

static void gen_prelude(CodeGenerator* self) {
    gen_line(self, "#include <stdint.h>");
    gen_line(self, "typedef uint8_t u8;");
    gen_line(self, "typedef uint16_t u16;");
    gen_line(self, "typedef uint32_t u32;");
    gen_line(self, "typedef uint64_t u64;");
    gen_line(self, "typedef int8_t i8;");
    gen_line(self, "typedef int16_t i16;");
    gen_line(self, "typedef int32_t i32;");
    gen_line(self, "typedef int64_t i64;");
}

void gen_code_for_ast(CodeGenerator* self, Ast* ast, const char* fpath) {
    self->ast = ast;
    self->code = null;

    gen_prelude(self);
    gen_function_decls(self);
    gen_functions(self);
    buf_push(self->code, '\0');

    FILE* file = fopen(fpath, "w");
    fwrite(self->code, sizeof(char), buf_len(self->code) - 1, file);
    fclose(file);
    printf("%s", self->code);
}
