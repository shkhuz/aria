#ifndef COMPILE_H
#define COMPILE_H

#include "core.h"
#include "msg.h"
#include "token.h"

// Context used for the whole compilation pipeline.
// Srcfile is per file.

typedef struct CompileCtx {
    Msg* msgs;
    bool print_msg_to_stderr;
    bool did_msg;

    Srcfile** srcfiles;
    bool parsing_error;
    stri interner;
} CompileCtx;

typedef struct {
    strid k;
    TokenKind v;
} KeywordMap;
extern KeywordMap* keywords;

CompileCtx compilectx_new();
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

#endif
