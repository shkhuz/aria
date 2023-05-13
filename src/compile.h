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
    DirectiveKind v;
} StringDirectiveKindTup;

struct AstNode;

typedef struct Srcfile Srcfile;

struct Srcfile {
    File handle;
    Token** tokens;
    struct AstNode** astnodes;
};

extern StringTokenKindTup* keywords;
extern StringDirectiveKindTup* directives;

typedef struct CompileCtx CompileCtx;

struct CompileCtx {
    struct Srcfile** srcfiles;
    usize num_srcfiles;
    Msg* msgs;
    bool parsing_error;
    bool print_msg_to_stderr;
    bool print_ast;
    bool did_msg;
};

void init_global_compiler_state();

CompileCtx compile_new_context();
void register_msg(CompileCtx* c, Msg msg);
void compile(CompileCtx* c);

#endif
