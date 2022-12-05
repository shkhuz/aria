#include "compile.h"
#include "core.h"
#include "buf.h"

CompileCtx compile_new_context() {
    CompileCtx c;
    c.msgs = NULL;
    return c;
}

void register_msg(CompileCtx* c, Msg msg) {
    bufpush(c->msgs, msg);
}
