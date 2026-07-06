#include <stdio.h>

#include "core.h"
#include "compile.h"

int main() {
    init_core();
    CompileCtx c = compilectx_from_stream(
            "imm a = some;\n"
            "imm b = struct(\"hiya\");\n"
            );
    compile(&c);
    if (c.parsing_error) compile_terminate(&c);
}
