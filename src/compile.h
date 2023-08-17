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

typedef struct {
    char* k;
    BuiltinSymbolKind v;
} StringBuiltinSymbolKindTup;

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
    struct Typespec* bool_type;
    struct Typespec* void_type;
    struct Typespec* noreturn_type;
} PredefTypespecs;

struct Srcfile {
    File handle;
    Token** tokens;
    struct AstNode** astnodes;
};

extern StringTokenKindTup* keywords;
extern StringBuiltinSymbolKindTup* builtin_symbols;
extern PredefTypespecs predef_typespecs;

typedef struct CompileCtx CompileCtx;
typedef struct Srcfile Srcfile;

struct CompileCtx {
    struct Typespec** mod_tys;
    const char* target_triple;

    Msg* msgs;
    bool parsing_error;
    bool sema_error;
    bool cg_error;

    bool print_msg_to_stderr;
    bool print_ast;
    bool did_msg;
};

void init_global_compiler_state();

CompileCtx compile_new_context(const char* target_triple);
void register_msg(CompileCtx* c, Msg msg);
void compile(CompileCtx* c);

struct Typespec* read_srcfile(const char* path, OptionalSpan span, CompileCtx* compile_ctx);

#endif
