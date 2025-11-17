#ifndef COMPILE_H
#define COMPILE_H

#include "core.h"

typedef struct {
    Msg* msgs;
    bool print_msg_to_stderr;
    bool did_msg;

    Srcfile srcfile;
    bool parsing_error;

    u64 next_srcfile_id;
} CompileCtx;

CompileCtx compilectx_new();
void compile_from_stream(CompileCtx* c, const char* stream);
// void compile_from_file(CompileCtx* c, const char* path);

#endif
