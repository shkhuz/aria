#include "parse.h"
#include "srcfile.h"
#include "msg.h"
#include "token.h"

ParseCtx parsectx_new(
    struct Srcfile* srcfile,
    struct CompileCtx* compilectx, 
    jmp_buf* error_handler_pos
) {
    ParseCtx p;
    p.srcfile = srcfile;
    p.srcfile->astnodes = NULL;
    p.current = p.srcfile->tokens[0];
    p.prev = NULL;
    p.token_idx = 0;
    p.compilectx = compilectx;
    p.error = false;
    p.error_handler_pos = error_handler_pos;
    return p;
}

void parse(ParseCtx* p) {
    // while (p->current->kind != TK_EOF) {

    // }
}
