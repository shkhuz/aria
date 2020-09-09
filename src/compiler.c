#include <ast_print.h>
#include <compiler.h>
#include <parser.h>
#include <lexer.h>
#include <token.h>
#include <error_msg.h>
#include <token_type.h>
#include <arpch.h>

Compiler compiler_new(const char* srcfile_path) {
    Compiler compiler;
    compiler.srcfile_path = srcfile_path;
}

Error compiler_run(Compiler* self) {
    File* srcfile = file_read(self->srcfile_path);
    if (!srcfile) {
        error_common(
                "cannot read `%s`: invalid filename or missing file",
                self->srcfile_path);
        return ERROR_READ;
    }

    Lexer lexer = lexer_new(srcfile);
    Error lexer_error = lexer_run(&lexer);
    if (lexer_error != ERROR_SUCCESS) return lexer_error;

/*     for (u64 t = 0; t < buf_len(lexer.tokens); t++) { */
/*         printf( */
/*                 "%lu, %lu, %u, %s\n", */
/*                 lexer.tokens[t]->line, */
/*                 lexer.tokens[t]->column, */
/*                 lexer.tokens[t]->type, */
/*                 lexer.tokens[t]->lexeme); */
/*     } */

    Parser parser = parser_new(srcfile, lexer.tokens);
    Error parser_error = parser_run(&parser);
    if (parser_error != ERROR_SUCCESS) return parser_error;

    print_ast(parser.stmts);

    return ERROR_SUCCESS;
}

