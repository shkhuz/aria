#include "arpch.h"
#include "aria.h"
#include "util/util.h"
#include "error_msg.h"

AstOutput build_ast(const char* fpath) {
    File* srcfile = file_read(fpath);
    if (!srcfile) {
        error_common(
                "cannot read `%s`: invalid filename or missing file",
                fpath);
        return (AstOutput){ true, null };
    }

    Lexer lexer;
    TokenOutput tokens = lex(&lexer, srcfile);
    if (tokens.error) {
        return (AstOutput){ true, null };
    }

    Parser parser;
    AstOutput ast = parse(&parser, srcfile, tokens.tokens);

    return (AstOutput){ false, ast.ast };
}

bool check_ast(Ast* ast) {
    Resolver resolver;
    bool error = resolve_ast(&resolver, ast);
    if (error) return error;

    TypeChecker type_checker;
    error = type_check_ast(&type_checker, ast);
    return error;
}
