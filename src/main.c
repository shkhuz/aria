#include "core.h"
#include "buf.h"
#include "types.h"
#include "printf.h"
#include "bigint.h"
#include "file_io.h"
#include "msg.h"
#include "cmd.h"
#include "lex.h"
#include "parse.h"

#include <getopt.h>

char* g_exec_path;

Srcfile** g_srcfiles = NULL;

Srcfile* read_srcfile(
        const char* path,
        MsgKind error_kind,
        Srcfile* error_srcfile,
        usize error_line,
        usize error_col,
        usize error_ch_count) {
    FileOrError efile = read_file(path);
    switch (efile.status) {
        case FOO_SUCCESS: {
            for (usize i = 0; i < buflen(g_srcfiles); i++) {
                if (strcmp(efile.handle.abs_path, g_srcfiles[i]->handle.abs_path) == 0)
                    return g_srcfiles[i];
            }
            Srcfile* srcfile = ALLOC_OBJ_FROM_TYPE(Srcfile);
            srcfile->handle = efile.handle;
            bufpush(g_srcfiles, srcfile);
            return srcfile;
        } break;

        case FOO_FAILURE: {
            msg(
                    error_kind,
                    error_srcfile,
                    error_line,
                    error_col,
                    error_ch_count,
                    "cannot read source file `%s%s%s`",
                    g_bold_color,
                    path,
                    g_reset_color);
        } break;

        case FOO_DIRECTORY: {
            msg(
                    error_kind,
                    error_srcfile,
                    error_line,
                    error_col,
                    error_ch_count,
                    "`%s%s%s` is a directory",
                    g_bold_color,
                    path,
                    g_reset_color);
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
    /*     aprintf("%d\n", buf[i]); */
    /* } */
    /* aprintf("%lu is a number with /%s/", UINT64_MAX, "www"); */
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
                aprintf(
                        "Aria language compiler\n"
                        "Usage: ariac [options] file...\n"
                        "\n"
                        "Options:\n"
                        "  -o, --output=<file>        Place the output into <file>\n"
                        "  --target=<triple>          Specify a target triple for cross compilation\n"
                        "  --help                     Display this help and exit\n"
                        "\n"
                        "To report bugs, please see:\n"
                        "<https://github.com/shkhuz/aria/issues/>\n"
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
        msg(
                MSG_KIND_ROOT_ERROR,
                NULL,
                0,
                0,
                0,
                "no input files");
        terminate_compilation();
    }

    bool read_error = false;
    for (int i = optind; i < argc; i++) {
        Srcfile* srcfile = read_srcfile(argv[i],
                                        MSG_KIND_ROOT_ERROR,
                                        NULL,
                                        0,
                                        0,
                                        0);
        if (!srcfile) {
            read_error = true;
            continue;
        }
    }
    if (read_error) terminate_compilation();

    bool parsing_error = false;
    bufloop(g_srcfiles, i) {
        LexContext l = lex_new_context(g_srcfiles[i]);
        lex(&l);
        if (l.error && !parsing_error) parsing_error = true;

        ParseContext p = parse_new_context(g_srcfiles[i]);
        parse(&p);
        if (p.error && !parsing_error) parsing_error = true;
    }
    if (parsing_error) terminate_compilation();
}

