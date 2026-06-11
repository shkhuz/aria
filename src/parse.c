#include "parse.h"
#include "srcfile.h"
#include "msg.h"
#include "token.h"
#include "ast.h"

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

static inline void msg_emit(ParseCtx* p, Msg* msg) {
    _msg_emit(msg, p->compilectx);
    if (msg->kind == MSG_ERROR) {
        p->error = true;
        longjmp(*p->error_handler_pos, 1);
    }
}

static inline void msg_emit_non_fatal(ParseCtx* p, Msg* msg) {
    _msg_emit(msg, p->compilectx);
    if (msg->kind == MSG_ERROR) {
        p->error = true;
    }
}

static void goto_next_token(ParseCtx* p) {
    if (p->token_idx < buflen(p->srcfile->tokens)) {
        p->token_idx++;
        p->prev = p->current;
        p->current = p->srcfile->tokens[p->token_idx];
    }
}

static void check_eof(ParseCtx* p, Token* pair) {
    if (p->current->kind == TK_EOF) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "unexpected end of file",
            p->current->span
        );
        msg_addl_fat(&msg, "while searching for:", pair->span);
        msg_emit(p, &msg);
    }
}

static bool match(ParseCtx* p, TokenKind kind) {
    if (p->current->kind == kind) {
        goto_next_token(p);
        return true;
    }
    return false;
}

static Token* expect(ParseCtx* p, TokenKind kind, const char* msgstr) {
    if (!match(p, kind)) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            msgstr,
            p->current->span
        );
        if (kind == TK_IDENT) {
            for (int i = 0; i < KEYWORDS_LEN; i++) {
                if (token_lexeme_eqlto(p->current, keywords[i].k)) {
                    msg_addl_thin(&msg, format_string("`%s` is a keyword", keywords[i].k));
                    break;
                }
            }
        }
        msg_emit(p, &msg);
        return NULL;
    }
    return p->prev;
}

static inline Token* expect_semicolon(ParseCtx* p) {
    return expect(p, TK_SEMICOLON, "expected `;`");
}

static Astnode* parse_vardecl(ParseCtx* p) {
    Token* keyword = p->prev;
    bool imm = true;
    if (keyword->kind == TK_KW_MUT) imm = false;
    Token* ident = expect(p, TK_IDENT, "expected variable name");
    expect_semicolon(p);
    return astnode_vardecl_new(
        keyword, 
        ident,
        NULL,
        NULL,
        NULL
    );
}

static Astnode* parse_astnode_root(ParseCtx* p) {
    if (match(p, TK_KW_IMM) || match(p, TK_KW_MUT)) {
        return parse_vardecl(p);
    }
}

void parse(ParseCtx* p) {
    while (p->current->kind != TK_EOF) {
        Astnode* astnode = parse_astnode_root(p);
        if (astnode) bufpush(p->srcfile->astnodes, astnode);
    }
}
