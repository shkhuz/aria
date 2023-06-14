#include "core.h"
#include "buf.h"
#include "bigint.h"
#include "file_io.h"
#include "msg.h"
#include "cmd.h"
#include "compile.h"

#include <getopt.h>

char* g_exec_path;

int main(int argc, char* argv[]) {
    {
        int* buf = NULL;
        bufpush(buf, -2);
        bufpush(buf, -3);
        bufpush(buf, 4);
        bufpush(buf, 4);
        bufpush(buf, 4);
        bufpush(buf, 2948);
        bufinsert(buf, 1, 999);
        assert(bufpop(buf) == 2948);
        assert(bufpop(buf) == 4);
        assert(bufpop(buf) == 4);
        assert(bufpop(buf) == 4);
        assert(bufpop(buf) == -3);
        assert(bufpop(buf) == 999);
        assert(bufpop(buf) == -2);
        assert(bufpop(buf) == 0);
        assert(bufpop(buf) == 0);
        assert(bufpop(buf) == 0);
    }

    {
        char* name = NULL;
        bufpush(name, 'h');
        bufstrexpandpush(name, "ello");
        bufstrexpandpush(name, ", world");
        bufpush(name, '!');
        bufpush(name, '\0');
        assert(strcmp(name, "hello, world!") == 0);
    }

    test_bigint();

    /* for (usize i = 0; i < buflen(buf); i++) { */
    /*     aria_printf("%d\n", buf[i]); */
    /* } */
    /* aria_printf("%lu is a number with /%s/", UINT64_MAX, "www"); */
    /* buffree(buf); */

    /* bigint a; */
    /* bigint_init_u64(&a, 1); */
    /* bigint_set_u64(&a, 1); */

    init_global_compiler_state();

    const char* outpath = "a.out";
    const char* target_triple = NULL;

    struct option options[] = {
        { "output", required_argument, 0, 'o' },
        { "target", required_argument, 0, 't' },
        { "help", no_argument, 0, 'h' },
        { 0, 0, 0, 0 },
    };

    while (true) {
        int longopt_idx = 0;
        int c = getopt_long(argc, argv, "o:", options, &longopt_idx);
        if (c == -1) break;

        switch (c) {
            case 'o': {
                outpath = optarg;
            } break;

            case 't': {
                target_triple = optarg;
            } break;

            case 'h': {
                printf(
                        "Aria language compiler\n"
                        "Usage: aria [options] file...\n"
                        "\n"
                        "Options:\n"
                        "  -o, --output=<file>        Place the output into <file>\n"
                        "  --target=<triple>          Specify a target triple for cross compilation\n"
                        "  --help                     Display this help and exit\n"
                        "\n"
                        );
                exit(0);
            } break;

            case '?': {
                exit(1);
            } break;

            default: {
            } break;
        }
    }

    CompileCtx compile_ctx = compile_new_context();
    compile_ctx.print_ast = true;

    if (optind == argc) {
        Msg msg = msg_with_no_span(MSG_ERROR, "no input files");
        _msg_emit(&msg, &compile_ctx);
        terminate_compilation();
    }

    bool read_error = false;
    for (int i = optind; i < argc; i++) {
        Typespec* mod = read_srcfile(argv[i], span_none(), &compile_ctx);
        if (!mod) {
            read_error = true;
            continue;
        }
    }
    if (read_error) terminate_compilation();

    compile(&compile_ctx);
    if (compile_ctx.parsing_error
        || compile_ctx.sema_error) {
        terminate_compilation();
    }
}
