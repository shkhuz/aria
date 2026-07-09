#include <stdio.h>

#include "core.h"
#include "compile.h"

int main() {
    init_core();
    CompileCtx c = compilectx_new();
    // compilectx_init_stream(
    //     &c, 
    //     "imm a = some;\n"
    //     "a:a,\n"
    //     "hi:struct(\"name\"),\n"
    //     "imm b = struct(\"hiya\");\n"
    // );
    compilectx_init_path(
        &c,
        "examples/v2-1.ar",
        span_none()
    );
    compile(&c);
    if (c.parsing_error) compile_terminate(&c);
}
