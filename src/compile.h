#ifndef COMPILE_H
#define COMPILE_H

#include "msg.h"

typedef struct CompileCtx CompileCtx;

struct CompileCtx {
    Msg* msgs;
};

CompileCtx compile_new_context();
void register_msg(CompileCtx* c, Msg msg);

#endif
