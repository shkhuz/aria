#include "core.h"
#include "aria.h"
#include "file_io.h"
#include "buf.h"
#include "msg.h"
#include "lex.h"
#include "parse.h"
#include "resolve.h"
#include "check.h"
#include "code_gen_linux-x64-nasm.h"
#include <getopt.h>

char* g_exec_path;

static Srcfile* read_srcfile(
        char* path,
        MsgKind error_kind,
        Srcfile* error_srcfile,
        size_t error_line,
        size_t error_column,
        size_t error_char_count) {

    FileOrError meta_file = read_file(path);
    switch (meta_file.status) {
        case FILE_ERROR_SUCCESS: {
            ALLOC_WITH_TYPE(srcfile, Srcfile);
            srcfile->handle = meta_file.handle;
            return srcfile;
        } break;

        case FILE_ERROR_FAILURE: {
            default_msg(
                    error_kind,
                    error_srcfile,
                    error_line,
                    error_column,
                    error_char_count,
                    "cannot read source file `{s}{s}{s}`",
                    g_bold_color,
                    path,
                    g_reset_color);
        } break;

        case FILE_ERROR_DIR: {
            default_msg(
                    error_kind,
                    error_srcfile,
                    error_line,
                    error_column,
                    error_char_count,
                    "`{s}{s}{s}` is a directory",
                    g_bold_color,
                    path,
                    g_reset_color);
        } break;
    }
    return null;
}

static void tests() {
    assert(align_to_pow2(7, 8) == 8);
    assert(align_to_pow2(1, 16) == 16);
    assert(align_to_pow2(8, 8) == 8);
    assert(align_to_pow2(0, 4) == 0);

    assert(strcmp(aria_strsub("Hello world", 6, 5), "world") == 0);
    assert(strcmp(aria_strsub("Hello world", 4, 3), "o w") == 0);
    assert(strcmp(aria_strapp("it", " works!"), "it works!") == 0);
    assert(strcmp(aria_basename("main.ar"), "main") == 0);
    assert(strcmp(aria_basename("src/examples/main.ar"), "src/examples/main") == 0);
    assert(strcmp(aria_notdir("main.ar"), "main.ar") == 0);
    assert(strcmp(aria_notdir("src/examples/main.ar"), "main.ar") == 0);
    assert(strcmp(aria_dir("src/examples/main.ar"), "src/examples/") == 0);
    assert(aria_dir("main.ar") == null);
}

int main(int argc, char* argv[]) {
    g_exec_path = argv[0];
    init_core();
    init_ds();
    tests();

    char* outpath = "a.out";
    struct option options[] = {
        { "output", required_argument, 0, 'o' },
        { 0 },
    };

    while (true) {
        int longopt_idx = 0;
        int c = getopt_long(argc, argv, "o:", options, &longopt_idx);
        if (c == -1) break;

        switch (c) {
            case 'o': {
                outpath = optarg;
            } break;
            
            case '?': {
                exit(1);
            } break;

            default: {
            } break;
        }
    }

    if (optind == argc) {
        default_msg(
                MSG_KIND_ROOT_ERROR,
                null, 
                0,
                0,
                0,
                "no input files");
        terminate_compilation();
    }

    Srcfile** srcfiles = null;
    bool read_error = false;
    for (int i = optind; i < argc; i++) {
        Srcfile* srcfile = read_srcfile(
                argv[i],
                MSG_KIND_ROOT_ERROR,
                null,
                0,
                0,
                0);
        if (!srcfile) {
            read_error = true;
            continue;
        }
        else {
            buf_push(srcfiles, srcfile);
        }
    }
    if (read_error) terminate_compilation();

    bool parsing_error = false;
    buf_loop(srcfiles, i) {
        LexContext l;
        l.srcfile = srcfiles[i];
        lex(&l);
        if (l.error) {
            parsing_error = true;
            continue;
        }
        /* else { */
        /*     buf_loop(srcfiles[i]->tokens, j) { */
        /*         aria_printf("token: {tk}\n", srcfiles[i]->tokens[j]); */
        /*     } */
        /* } */

        ParseContext p;
        p.srcfile = srcfiles[i];
        parse(&p);
        if (p.error) parsing_error = true;
    }
    if (parsing_error) terminate_compilation();

    bool resolving_error = false;
    buf_loop(srcfiles, i) {
        ResolveContext r;
        r.srcfile = srcfiles[i];
        resolve(&r);
        if (r.error) resolving_error = true;
    }
    if (resolving_error) terminate_compilation();

    bool checking_error = false;
    buf_loop(srcfiles, i) {
        CheckContext c;
        c.srcfile = srcfiles[i];
        check(&c);
        if (c.error) checking_error = true;
    }
    if (checking_error) terminate_compilation();

    CodeGenContext* gen_ctxs = null;
    buf_loop(srcfiles, i) {
        CodeGenContext c;
        c.srcfile = srcfiles[i];
        code_gen(&c, outpath);
        buf_push(gen_ctxs, c);
    }
    code_gen_output_bin(gen_ctxs, outpath);
}
