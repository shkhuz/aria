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
    p.srcfile->ast = NULL;
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

static void goto_prev_token(ParseCtx* p) {
    if (p->token_idx > 0) {
        p->token_idx--;
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

static Token* expect_rparen(ParseCtx* p) {
    return expect(p, TK_RPAREN, "expected `)`");
}

static inline Token* expect_semicolon(ParseCtx* p) {
    return expect(p, TK_SEMICOLON, "expected `;`");
}

static inline Token* expect_comma(ParseCtx* p) {
    return expect(p, TK_COMMA, "expected `,`");
}

static Astnode* parse_atom_expr(ParseCtx* p) {
    if (match(p, TK_IDENT)) {
        Astnode* left = astnode_symbol_new(p->prev);
        return left;
    }
    else if (match(p, TK_KW_STRUCT)) {
        Token* keyword = p->prev;
        if (match(p, TK_LPAREN)) {
            Token* paren = p->prev;
            Token* path = expect(
                p, 
                TK_STRLIT, 
                "expected a string literal path"
            );
            expect_rparen(p);
            return astnode_struct_import_new(keyword, path, p->prev);
        } 
        else if (match(p, TK_LBRACE)) {

        }
    }

    Msg msg = msg_with_span(
        MSG_ERROR,
        p->current->kind == TK_SEMICOLON
            ? "unexpected `;`"
            : "expected expression",
        p->current->span
    );
    msg_emit(p, &msg);
    return NULL;
}

static Astnode* parse_vardecl(ParseCtx* p) {
    Token* keyword = p->prev;
    bool imm = true;
    if (keyword->kind == TK_KW_MUT) imm = false;
    Token* ident = expect(p, TK_IDENT, "expected variable name");
    Astnode* initializer = NULL;
    if (match(p, TK_EQUAL)) {
        initializer = parse_atom_expr(p);
    }
    expect_semicolon(p);
    return astnode_vardecl_new(
        keyword, 
        ident,
        NULL,
        NULL,
        initializer
    );
}

static Astnode* parse_astnode_root(ParseCtx* p) {
    if (match(p, TK_KW_IMM) || match(p, TK_KW_MUT)) {
        return parse_vardecl(p);
    } 
    else if (match(p, TK_IDENT)) {
        Token* ident = p->prev;
        if (match(p, TK_COLON)) {
            Astnode* type = parse_atom_expr(p);
            if (p->current->kind != TK_RBRACE) {
                expect_comma(p);
            }
            return astnode_field_new(ident, type);
        }
        else {
            Msg msg = msg_with_span(
                MSG_ERROR,
                "expected `:` for field declaration",
                p->current->span
            );
            msg_emit(p, &msg);
            // Not needed because we fatally exit.
            // But just for completeness.
            goto_prev_token(p);
        }
    }
    else {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "expected top-level declaration",
            p->current->span
        );
        msg_emit(p, &msg);
    }
}

void parse(ParseCtx* p) {
    while (p->current->kind != TK_EOF) {
        Astnode* astnode = parse_astnode_root(p);
        if (astnode) bufpush(p->srcfile->ast, astnode);
    }
}
