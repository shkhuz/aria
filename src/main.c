#include <stdio.h>

#include "core.h"
#include "compile.h"

int main() {
    init_core();
    CompileCtx c = compilectx_from_stream("const; wow; fn main");
    compile(&c);
    if (c.parsing_error) compile_terminate(&c);
}
