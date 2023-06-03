#ifndef COMPILE_H
#define COMPILE_H

#include "msg.h"
#include "core.h"
#include "file_io.h"
#include "token.h"
#include "ast.h"

typedef struct {
    char* k;
    TokenKind v;
} StringTokenKindTup;

struct AstNode;
struct Typespec;

typedef struct {
    struct Typespec* u8_type;
    struct Typespec* u16_type;
    struct Typespec* u32_type;
    struct Typespec* u64_type;
    struct Typespec* i8_type;
    struct Typespec* i16_type;
    struct Typespec* i32_type;
    struct Typespec* i64_type;
    struct Typespec* void_type;
} PredefTypespecs;

typedef struct Srcfile Srcfile;

struct Srcfile {
    File handle;
    Token** tokens;
    struct AstNode** astnodes;
};

extern StringTokenKindTup* keywords;
extern PredefTypespecs predef_typespecs;

typedef struct CompileCtx CompileCtx;

struct CompileCtx {
    struct Srcfile** srcfiles;
    usize num_srcfiles;
    Msg* msgs;
    bool parsing_error;
    bool sema_error;
    bool print_msg_to_stderr;
    bool print_ast;
    bool did_msg;
};

void init_global_compiler_state();

CompileCtx compile_new_context();
void register_msg(CompileCtx* c, Msg msg);
void compile(CompileCtx* c);

Srcfile* read_srcfile(const char* path, OptionalSpan span, CompileCtx* compile_ctx);

#endif
