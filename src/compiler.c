#include <ast_print.h>
#include <compiler.h>
#include <linker.h>
#include <parser.h>
#include <lexer.h>
#include <token.h>
#include <error_msg.h>
#include <token_type.h>
#include <arpch.h>

Compiler compiler_new(const char* srcfile_path) {
    Compiler compiler;
    compiler.srcfile_path = srcfile_path;
    return compiler;
}

CompileOutput compiler_run(Compiler* self) {
    File* srcfile = file_read(self->srcfile_path);
    if (!srcfile) {
        error_common(
                "cannot read `%s`: invalid filename or missing file",
                self->srcfile_path);
        return (CompileOutput){ ERROR_READ, null };
    }

    Lexer lexer = lexer_new(srcfile);
    Error lexer_error = lexer_run(&lexer);
    if (lexer_error != ERROR_SUCCESS) {
        return (CompileOutput){ lexer_error, null };
    }

/*     buf_loop(lexer.tokens, t) { */
/*         printf( */
/*                 "%lu, %lu, %u, %s\n", */
/*                 lexer.tokens[t]->line, */
/*                 lexer.tokens[t]->column, */
/*                 lexer.tokens[t]->type, */
/*                 lexer.tokens[t]->lexeme); */
/*     } */

    Parser* parser = malloc(sizeof(Parser));
    *parser = parser_new(srcfile, lexer.tokens);
    Error parser_error = parser_run(parser);
    if (parser_error != ERROR_SUCCESS) {
        return (CompileOutput){ parser_error, null };
    }

    return (CompileOutput){ ERROR_SUCCESS, parser };
}

bool compiler_post_run_all(Parser** parsers) {
    bool error = false;
    buf_loop(parsers, p) {
        Linker linker = linker_new(parsers[p]->srcfile, parsers[p]->stmts);
        Error linker_error = linker_run(&linker);
        if (linker_error != ERROR_SUCCESS) {
            error = true;
            continue;
        }
    }
    return error;
}
