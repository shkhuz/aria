#include <stdio.h>

#include "compile.h"

int main() {
    CompileCtx c = compilectx_from_stream("const; wow; fn main");
    compile(&c);
    if (c.parsing_error) compile_terminate(&c);
}
