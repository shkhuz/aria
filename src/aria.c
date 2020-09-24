#include "arpch.h"
#include "util/util.h"
#include "ds/ds.h"
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

    keywords_init();
    builtin_types_init();

    Ast** asts = null;
    bool error = false;
    for (int f = 1; f < argc; f++) {
        AstOutput ast = build_ast(argv[f]);
        if (!ast.error) buf_push(asts, ast.ast);
        if (!error) error = ast.error;
    }

    if (error) {
        exit_msg(2, "compilation aborted");
    }

    error = false;
    buf_loop(asts, ast) {
        bool current_error = check_ast(asts[ast]);
        if (!error) error = current_error;
    }

    if (error) {
        exit_msg(3, "compilation aborted");
    }

    char** output_code_fpaths = null;
    buf_loop(asts, ast) {
        CodeGenerator code_generator;
        char* output_code_fpath =
                not_dir(change_ext_for_path(asts[ast]->srcfile->fpath, "c"));
        buf_push(output_code_fpaths, output_code_fpath);
        gen_code_for_ast(
                &code_generator,
                asts[ast],
                output_code_fpath
        );
    }

    char* output_code_fpaths_concat = "gcc ";
    buf_loop(output_code_fpaths, f) {
        output_code_fpaths_concat =
            concat(
                output_code_fpaths_concat,
                output_code_fpaths[f]
            );
    }

    system(output_code_fpaths_concat);
}

