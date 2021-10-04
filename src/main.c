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
#include <argp.h>

typedef struct {
    char** inpaths;
    char* outpath;
} Arguments;

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
                    "cannot read source file `" ANSI_FBOLD "%s" ANSI_RESET "`",
                    path);
        } break;

        case FILE_ERROR_DIR: {
            default_msg(
                    error_kind,
                    error_srcfile,
                    error_line,
                    error_column,
                    error_char_count,
                    "`" ANSI_FBOLD "%s" ANSI_RESET "` is a directory",
                    path);
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

static error_t parse_cmdopts(int key, char* arg, struct argp_state* state) {
    Arguments* args = state->input;
    switch (key) {
        case 'o': {
            args->outpath = arg;
        } break;

        case ARGP_KEY_ARG: {
            buf_push(args->inpaths, arg);
        } break;

        case ARGP_KEY_END: {
            if (state->arg_num < 1) {
                argp_failure(state, 1, 0, "no input file(s)");
            }
        } break;

        default: return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    g_exec_path = argv[0];
    init_ds();
    tests();

    Arguments args;
    args.inpaths = null;
    args.outpath = "a.out";
    struct argp_option options[] = {
        { "output", 'o', "FILE", 0, "Place the output into FILE", 0 },
        { 0 },
    };
    struct argp argp = { options, parse_cmdopts, "FILE...", 0, 0, 0, 0, };
    argp_parse(&argp, argc, argv, 0, 0, &args);

    Srcfile** srcfiles = null;
    bool read_error = false;
    buf_loop(args.inpaths, i) {
        Srcfile* srcfile = read_srcfile(
                args.inpaths[i],
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
        code_gen(&c, args.outpath);
        buf_push(gen_ctxs, c);
    }
    code_gen_output_bin(gen_ctxs, args.outpath);
}
