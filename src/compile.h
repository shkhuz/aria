#ifndef COMPILE_H
#define COMPILE_H

#include "core.h"
#include "msg.h"

// Context used for the whole compilation pipeline.
// Srcfile is per file.

typedef struct CompileCtx {
    Msg* msgs;
    bool print_msg_to_stderr;
    bool did_msg;

    Srcfile* srcfiles;
    bool parsing_error;

    u64 next_srcfile_id;
} CompileCtx;

CompileCtx compilectx_new();
Srcfile* read_srcfile(
    CompileCtx* c, 
    const char* path, 
    OptionalSpan span
);
void compilectx_init_stream(CompileCtx* c, const char* stream);
bool compilectx_init_path(
    CompileCtx* c, 
    const char* path, 
    OptionalSpan span
);
void compile(CompileCtx* c);
void compile_register_msg(CompileCtx* c, Msg msg);
void compile_terminate(CompileCtx* c);

#endif
