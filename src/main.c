#include "core.h"
#include "aria.h"
#include "file_io.h"
#include "buf.h"
#include "msg.h"
#include "lex.h"
#include "parse.h"
#include "resolve.h"

char* g_exec_path;

Srcfile* read_srcfile(
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

int main(int argc, char* argv[]) {
    g_exec_path = argv[0];
    init_ds();

    if (argc < 2) {
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
    for (int i = 1; i < argc; i++) {
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
        else {
            /* buf_loop(srcfiles[i]->tokens, j) { */
            /*     aria_printf("token: {tk}\n", srcfiles[i]->tokens[j]); */
            /* } */
        }

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
}
