#include <stdio.h>

#include "core.h"
#include "compile.h"

int main() {
    int* a = NULL;
    bufpush(a, 1);
    bufpush(a, 2);
    assert(bufpop(a) == 2);
    assert(bufpop(a) == 1);
    assert(buflen(a) == 0);


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
        "examples/v2-1.ar"
    );
    compile(&c);
    stri_print_stats(&c.interner);
    if (c.parsing_error) compile_terminate(&c);
}
