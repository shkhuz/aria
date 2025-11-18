#ifndef COMPILE_H
#define COMPILE_H

#include "core.h"
#include "msg.h"

typedef struct CompileCtx {
    Msg* msgs;
    bool print_msg_to_stderr;
    bool did_msg;

    Srcfile srcfile;
    bool parsing_error;

    u64 next_srcfile_id;
} CompileCtx;

CompileCtx compilectx_from_stream(const char* stream);
//CompileCtx compilectx_from_path(const char* path);
void compile(CompileCtx* c);
void compile_register_msg(CompileCtx* c, Msg msg);
void compile_terminate(CompileCtx* c);

#endif
