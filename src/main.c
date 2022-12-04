#include "core.h"
#include "buf.h"
#include "misc.h"
#include "printf.h"
#include "bigint.h"
#include "file_io.h"
#include "msg.h"
#include "cmd.h"
#include "lex.h"
#include "ast.h"
#include "parse.h"
#include "ast_print.h"

#include <getopt.h>

char* g_exec_path;

Srcfile** g_srcfiles = NULL;

Srcfile* read_srcfile(const char* path, OptionalSpan span) {
    FileOrError efile = read_file(path);
    switch (efile.status) {
        case FOO_SUCCESS: {
            for (usize i = 0; i < buflen(g_srcfiles); i++) {
                if (strcmp(efile.handle.abs_path, g_srcfiles[i]->handle.abs_path) == 0)
                    return g_srcfiles[i];
            }
            Srcfile* srcfile = malloc(sizeof(Srcfile));
            srcfile->handle = efile.handle;
            bufpush(g_srcfiles, srcfile);
            return srcfile;
        } break;

        case FOO_FAILURE: {
            const char* error_msg = aria_format("cannot read source file `%s`", path);
            if (span.exists) {
                Msg msg = msg_with_span(MSG_ERROR, error_msg, span.span);
                _msg_emit(&msg);
            } else {
                Msg msg = msg_with_no_span(MSG_ERROR, error_msg);
                _msg_emit(&msg);
            }
        } break;

        case FOO_DIRECTORY: {
            const char* error_msg = aria_format("`%s` is a directory", path);
            if (span.exists) {
                Msg msg = msg_with_span(MSG_ERROR, error_msg, span.span);
                _msg_emit(&msg);
            } else {
                Msg msg = msg_with_no_span(MSG_ERROR, error_msg);
                _msg_emit(&msg);
            }
        } break;
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    /* int* buf = NULL; */
    /* bufpush(buf, -2); */
    /* bufpush(buf, -3); */
    /* bufpush(buf, 4); */
    /* bufpush(buf, 2948); */

    /* for (usize i = 0; i < buflen(buf); i++) { */
    /*     aria_printf("%d\n", buf[i]); */
    /* } */
    /* aria_printf("%lu is a number with /%s/", UINT64_MAX, "www"); */
    /* buffree(buf); */

    /* bigint a; */
    /* bigint_init_u64(&a, 1); */
    /* bigint_set_u64(&a, 1); */
    init_types();

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

    if (optind == argc) {
        Msg msg = msg_with_no_span(MSG_ERROR, "no input files");
        _msg_emit(&msg);
        terminate_compilation();
    }

    bool read_error = false;
    for (int i = optind; i < argc; i++) {
        Srcfile* srcfile = read_srcfile(argv[i], span_none());
        if (!srcfile) {
            read_error = true;
            continue;
        }
    }
    if (read_error) terminate_compilation();

    bool parsing_error = false;
    bufloop(g_srcfiles, i) {
        LexCtx l = lex_new_context(g_srcfiles[i]);
        lex(&l);
        if (l.error) {
            parsing_error = true;
            continue;
        }

        ParseCtx p = parse_new_context(g_srcfiles[i]);
        parse(&p);
        if (p.error) parsing_error = true;
        else ast_print(p.srcfile->astnodes);
    }
    if (parsing_error) terminate_compilation();
}

