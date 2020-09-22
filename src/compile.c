#include "arpch.h"
#include "aria.h"
#include "util/util.h"
#include "error_msg.h"

AstOutput build_ast(Compiler* self, const char* fpath) {
    self->fpath = fpath;

    File* srcfile = file_read(self->fpath);
    if (!srcfile) {
        error_common(
                "cannot read `%s`: invalid filename or missing file",
                self->fpath);
        return (AstOutput){ true, null };
    }

    Lexer lexer;
    bool lexer_error = lex(&lexer, srcfile);
    if (lexer_error) {
        return (AstOutput){ true, null };
    }

    return (AstOutput){ false, null };
}

