#include <parser.h>
#include <arpch.h>

Parser parser_new(File* srcfile, Token** tokens) {
    Parser parser;
    parser.srcfile = srcfile;
    parser.tokens = tokens;
    parser.tokens_idx = 0;
    parser.stmts = null;
    return parser;
}

static Token* current(Parser* p) {
    return p->tokens[p->tokens_idx];
}

static void goto_next_token(Parser* p) {
    if (p->tokens_idx < buf_len(p->tokens)) {
        p->tokens_idx++;
    }
}

static Stmt* decl(Parser* p) {
    goto_next_token(p);
    return null;
}

void parser_run(Parser* p) {
    while (current(p)->type != T_EOF) {
        Stmt* stmt = decl(p);
        buf_push(p->stmts, stmt);
    }
}
