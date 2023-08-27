#include "core.h"
#include "buf.h"
#include "bigint.h"
#include "file_io.h"
#include "msg.h"
#include "cmd.h"
#include "compile.h"

#include <getopt.h>

char g_exec_path[PATH_MAX+1];
char* g_lib_path;

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
    init_bigint();

    {
        ssize_t len = readlink("/proc/self/exe", g_exec_path, PATH_MAX);
        if (len == -1) {
            fprintf(stderr, "readlink(): cannot get executable path");
            exit(1);
        }
        g_exec_path[len] = '\0';
        g_lib_path = NULL;
        bufstrexpandpush(g_lib_path, g_exec_path);
        // Remove executable name
        while (g_lib_path[buflen(g_lib_path)-1] != '/') bufpop(g_lib_path);
        bufpop(g_lib_path);
        //  Remove parent directory
        while (g_lib_path[buflen(g_lib_path)-1] != '/') bufpop(g_lib_path);
        bufstrexpandpush(g_lib_path, "lib/aria/");
        bufpush(g_lib_path, '\0');
    }

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

    CompileCtx compile_ctx = compile_new_context(target_triple);
    compile_ctx.print_ast = false;

    if (optind == argc) {
        Msg msg = msg_with_no_span(MSG_ERROR, "no input files");
        _msg_emit(&msg, &compile_ctx);
        terminate_compilation(&compile_ctx);
    }

    bool read_error = false;
    for (int i = optind; i < argc; i++) {
        Typespec* mod = read_srcfile(argv[i], NULL, span_none(), &compile_ctx);
        if (!mod) {
            read_error = true;
            continue;
        }
    }
    if (read_error) terminate_compilation(&compile_ctx);

    compile(&compile_ctx);
    if (compile_ctx.parsing_error
        || compile_ctx.sema_error
        || compile_ctx.cg_error
        || compile_ctx.compile_error) {
        terminate_compilation(&compile_ctx);
    }
}
