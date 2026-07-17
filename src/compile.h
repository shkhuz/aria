#ifndef COMPILE_H
#define COMPILE_H

#include "core.h"
#include "msg.h"
#include "token.h"
#include "node.h"

// Context used for the whole compilation pipeline.
// Srcfile is per file.

typedef struct CompileCtx {
    Msg* msgs;
    bool print_msg_to_stderr;
    bool did_msg;

    DefineList(srcfiles, Srcfile*);
    bool parsing_error;
    Arena permarena;
    Arena parsearena;
    stri interner;
} CompileCtx;

typedef struct {
    strid k;
    TokenKind v;
} StrTokenMap;

extern DefineList(keywords, StrTokenMap);
extern DefineList(directives, StrTokenMap);

int read_srcfile(
    CompileCtx* c, 
    const char* path, 
    Span span,
    Srcfile* spansrc
);
int compilectx_init_stream(CompileCtx* c, const char* stream);
int compilectx_init_path(CompileCtx* c, const char* path);
void compile(CompileCtx* c);
void compile_register_msg(CompileCtx* c, Msg msg);
void compile_terminate(CompileCtx* c);

static inline Token* tk(Srcfile* src, TokenIndex idx) {
    return &listget(src->tokens, idx);
}

static inline Node* nd(Srcfile* src, NodeIndex idx) {
    return &listget(src->nodes, idx);
}

#endif
