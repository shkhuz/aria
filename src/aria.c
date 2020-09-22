#include "arpch.h"
#include "aria.h"

#define exit_msg(code, msg, ...) \
    fprintf(stderr, "aria: "), \
    fprintf(stderr, msg, ##__VA_ARGS__), \
    fprintf(stderr, "\n"), \
    exit(code)

int main(int argc, char** argv) {
    if (argc < 2) {
        exit_msg(1, "no source files supplied: exiting");
    }

    for (int f = 1; f < argc; f++) {
        Compiler compiler;
        AstOutput ast = build_ast(&compiler, argv[f]);
    }
}

