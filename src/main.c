#include "core.h"
#include "buf.h"
#include "bigint.h"
#include "file_io.h"
#include "msg.h"
#include "cmd.h"
#include "compile.h"

#include <getopt.h>

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

    const char* outpath = NULL;
    const char* target_triple = NULL;
    bool naked = false;

    struct option options[] = {
        { "output", required_argument, 0, 'o' },
        { "target", required_argument, 0, 't' },
        { "naked",  no_argument, 0, 0 },
        { "help",   no_argument, 0, 'h' },
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

            case 0: {
                naked = true;
            } break;

            case 'h': {
                printf(
                        "Aria language compiler\n"
                        "Usage: aria [options] file...\n"
                        "\n"
                        "Options:\n"
                        "  -o, --output=<file>        Place the output into <file>\n"
                        "  -t, --target=<triple>      Specify a target triple for cross compilation\n"
                        "  --naked                    Emit an object file, instead of an executable, with no runtime\n"
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

    CompileCtx compile_ctx = compile_new_context(
        outpath,
        target_triple,
        naked);
    compile_ctx.print_ast = false;

    if (optind == argc) {
        Msg msg = msg_with_no_span(MSG_ERROR, "no input files");
        _msg_emit(&msg, &compile_ctx);
        terminate_compilation(&compile_ctx);
    }

    bool read_error = false;
    for (int i = optind; i < argc; i++) {
        if (strstr(argv[i], ".o") != NULL) {
            if (naked) {
                Msg msg = msg_with_no_span(MSG_ERROR, "object files supplied when '--naked' set");
                _msg_emit(&msg, &compile_ctx);
                terminate_compilation(&compile_ctx);
            }
            bufpush(compile_ctx.other_obj_files, argv[i]);
        } else {
            Typespec* mod = read_srcfile(argv[i], NULL, span_none(), &compile_ctx);
            if (!mod) {
                read_error = true;
                continue;
            }
        }
    }

    if (!naked) {
        Typespec* mod = read_srcfile("core", "core", span_none(), &compile_ctx);
        if (!mod) {
            read_error = true;
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
