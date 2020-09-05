#include <compiler.h>
#include <parser.h>
#include <lexer.h>
#include <token.h>
#include <error_value.h>
#include <error_msg.h>
#include <arpch.h>

Compiler compiler_new(const char* srcfile_path) {
    Compiler compiler;
    compiler.srcfile_path = srcfile_path;
}

int compiler_run(Compiler* self) {
    File* srcfile = file_read(self->srcfile_path);
    if (!srcfile) {
        error_common(
                "cannot read `%s`: invalid filename or missing file",
                self->srcfile_path);
        return ERROR_READ;
    }

    Lexer lexer = lexer_new(srcfile);
    lexer_run(&lexer);

    Parser parser = parser_new(srcfile, lexer.tokens);
    parser_run(&parser);

    return ERROR_SUCCESS;
}

