#include "parse.h"
#include "srcfile.h"
#include "msg.h"
#include "token.h"
#include "ast.h"
#include "compile.h"

#include "ast_builder.h"

static Astnode* parse_astnode_root(ParseCtx* p);
static Astnode* parse_block(ParseCtx* p);

#define msg_with_span(kind, msg, span) _msg_with_span(kind, msg, span, p->src)
#define msg_addl_fat(m, msg, span) _msg_addl_fat(m, msg, span, p->src)

static inline Token* at(ParseCtx* p, TokenIndex idx) {
    return &p->src->tokens[idx];
}

static inline Token* current(ParseCtx* p) {
    return at(p, p->token_idx);
}

static inline Token* prev(ParseCtx* p) {
    if (p->token_idx > 0) return at(p, p->token_idx - 1);
    assert(0);
    return NULL;
}

ParseCtx parsectx_new(
    struct Srcfile* src,
    struct CompileCtx* compilectx, 
    jmp_buf* error_handler_pos
) {
    ParseCtx p;
    p.src = src;
    p.src->ast = NULL;
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
    if (p->token_idx < buflen(p->src->tokens)) {
        p->token_idx++;
    }
}

static void goto_prev_token(ParseCtx* p) {
    if (p->token_idx > 0) {
        p->token_idx--;
    }
}

static void check_eof(ParseCtx* p, TokenIndex pair) {
    if (current(p)->kind == TK_EOF) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "unexpected end of file",
            current(p)->span
        );
        msg_addl_fat(&msg, "while searching for:", at(p, pair)->span);
        msg_emit(p, &msg);
    }
}

static bool match(ParseCtx* p, TokenKind kind) {
    if (current(p)->kind == kind) {
        goto_next_token(p);
        return true;
    }
    return false;
}

static TokenIndex expect(ParseCtx* p, TokenKind kind, const char* msgstr) {
    if (!match(p, kind)) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            msgstr,
            current(p)->span
        );
        if (kind == TK_IDENT) {
            for (int i = 0; i < KEYWORDS_LEN; i++) {
                if (token_lexeme_eqlto(current(p), p->src, keywords[i].k)) {
                    msg_addl_thin(
                        &msg, 
                        format_string("`%s` is a keyword", keywords[i].k)
                    );
                    break;
                }
            }
        }
        msg_emit(p, &msg);
        // Dead code.
        return -1;
    }
    return p->token_idx-1;
}

static TokenIndex expect_lparen(ParseCtx* p) {
    return expect(p, TK_LPAREN, "expected `(`");
}

static TokenIndex expect_rparen(ParseCtx* p) {
    return expect(p, TK_RPAREN, "expected `)`");
}

static TokenIndex expect_lbrace(ParseCtx* p) {
    return expect(p, TK_LBRACE, "expected `{`");
}

static inline TokenIndex expect_colon(ParseCtx* p) {
    return expect(p, TK_COLON, "expected `:`");
}

static inline TokenIndex expect_semicolon(ParseCtx* p) {
    return expect(p, TK_SEMICOLON, "expected `;`");
}

static inline TokenIndex expect_comma(ParseCtx* p) {
    return expect(p, TK_COMMA, "expected `,`");
}

static Astnode* parse_atom_expr(ParseCtx* p) {
    if (match(p, TK_IDENT)) {
        Astnode* left = astnode_symbol_new(p, p->token_idx-1);
        return left;
    } else if (match(p, TK_KW_COMP)) {
        TokenIndex keyword = p->token_idx-1;
        Astnode* child = parse_atom_expr(p);
        return astnode_comp_new(p, keyword, child);
    } else if (match(p, TK_KW_STRUCT)) {
        TokenIndex keyword = p->token_idx-1;
        if (match(p, TK_LPAREN)) {
            TokenIndex path = expect(
                p, 
                TK_STRLIT, 
                "expected a string literal path"
            );
            expect_rparen(p);

            if (token_lexeme_eqlto(at(p, path), p->src, "\"\"")) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "empty path",
                    at(p, path)->span
                );
                msg_emit_non_fatal(p, &msg);
                return NULL;
            }

            StrlitData* data = &token_strlit_data[at(p, path)->extra];
            char path_wc[1024];
            const char* this_path = p->src->handle.path;
            const char* last_fslash = strrchr(this_path, '/');
            int start = 0;
            if (last_fslash) {
                start = last_fslash - this_path + 1;
                memcpy(path_wc, this_path, start);
            }
            memcpy(
                &path_wc[start],
                data->str,
                data->len
            );
            path_wc[start + data->len] = '\0';
            Srcfile* src = read_srcfile(
                p->compilectx,
                path_wc,
                at(p, path)->span,
                p->src
            );
            return astnode_struct_import_new(
                p, 
                keyword, 
                path, 
                p->token_idx-1,
                src
            );
        } else if (match(p, TK_LBRACE)) {
            TokenIndex lbrace = p->token_idx-1;
            Astnode** ast = NULL;
            while (!match(p, TK_RBRACE)) {
                check_eof(p, lbrace);
                Astnode* n = parse_astnode_root(p);
                if (n) bufpush(ast, n);
            }
            return astnode_struct_inline_new(
                p, 
                keyword,
                ast,
                p->token_idx-1
            );
        }
    } else if (current(p)->kind == TK_LBRACE) {
        return parse_block(p);
    }

    Msg msg = msg_with_span(
        MSG_ERROR,
        current(p)->kind == TK_SEMICOLON
            ? "unexpected `;`"
            : "expected expression",
        current(p)->span
    );
    msg_emit(p, &msg);
    return NULL;
}

static Astnode* parse_vardecl(ParseCtx* p) {
    TokenIndex keyword = p->token_idx-1;
    bool imm = true;
    if (at(p, keyword)->kind == TK_KW_MUT) imm = false;
    TokenIndex ident = expect(p, TK_IDENT, "expected variable name");
    Astnode* type = NULL;
    if (match(p, TK_COLON)) {
        type = parse_atom_expr(p);
    }
    Astnode* initializer = NULL;
    if (match(p, TK_EQUAL)) {
        initializer = parse_atom_expr(p);
    }
    expect_semicolon(p);
    return astnode_vardecl_new(
        p, 
        keyword, 
        ident,
        type,
        -1,
        initializer
    );
}

static Astnode* parse_func(ParseCtx* p) {
    TokenIndex keyword = p->token_idx-1;
    TokenIndex ident = expect(p, TK_IDENT, "expected function name");
    TokenIndex lparen = expect_lparen(p);
    Astnode** params = NULL;
    while (!match(p, TK_RPAREN)) {
        check_eof(p, lparen);
        TokenIndex pident = expect(
            p, 
            TK_IDENT, 
            "expected parameter name"
        );
        expect_colon(p);
        Astnode* ptype = parse_atom_expr(p);
        bufpush(params, astnode_param_new(p, pident, ptype));
        if (current(p)->kind != TK_RPAREN) {
            expect_comma(p);
        }
    }
    Astnode* returntype = parse_atom_expr(p);
    if (current(p)->kind != TK_LBRACE && returntype->kind == AST_BLOCK) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "expected `{` for function body",
            current(p)->span
        );
        msg_addl_fat(
            &msg, 
            "perhaps you forgot the return type?", 
            span_only_firstchar(returntype->span)
        );
        msg_emit(p, &msg);
    }
    Astnode* body = parse_block(p);
    return astnode_func_new(
        p, 
        keyword,
        ident,
        params,
        returntype,
        body
    );
}

static Astnode* parse_astnode_root(ParseCtx* p) {
    if (match(p, TK_KW_IMM) || match(p, TK_KW_MUT)) {
        return parse_vardecl(p);
    } else if (match(p, TK_KW_FUN)) {
        return parse_func(p); 
    } else if (match(p, TK_IDENT)) {
        TokenIndex ident = p->token_idx-1;
        if (match(p, TK_COLON)) {
            Astnode* type = parse_atom_expr(p);
            if (current(p)->kind != TK_RBRACE) {
                expect_comma(p);
            }
            return astnode_field_new(p, ident, type);
        } else {
            Msg msg = msg_with_span(
                MSG_ERROR,
                "expected `:` for field declaration",
                current(p)->span
            );
            msg_emit(p, &msg);
            // Not needed because we fatally exit.
            // But just for completeness.
            goto_prev_token(p);
        }
    } else {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "expected top-level declaration",
            current(p)->span
        );
        msg_emit(p, &msg);
    }
}

static Astnode* parse_block(ParseCtx* p) {
    TokenIndex lbrace = expect_lbrace(p);
    Astnode** ast = NULL;
    Astnode* value = NULL;

    while (!match(p, TK_RBRACE)) {
        check_eof(p, lbrace);
        Astnode* child = NULL;
        if (match(p, TK_KW_IMM) || match(p, TK_KW_MUT)) {
            child = parse_vardecl(p); 
        } else if (match(p, TK_KW_FUN)) {
            child = parse_func(p);
        } else if (match(p, TK_KW_YIELD)) {
            value = parse_atom_expr(p);
            expect_semicolon(p);
            if (!match(p, TK_RBRACE)) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "expected `}`",
                    current(p)->span
                );
                msg_addl_thin(&msg, "`yield` must be last in a block");
                msg_emit(p, &msg);
            }
            break;
        } else {
            Astnode* n = parse_atom_expr(p);
            // The type of AST that warrants skipping the semicolon 
            // should have a child of kind AST_BLOCK or itself be AST_BLOCK.
            // Nodes having child at the end:
            // - AST_COMP
            // - AST_IF
            if (n->kind == AST_BLOCK
                || (n->kind == AST_COMP && n->comp.child->kind == AST_BLOCK)) {
            } else {
                if (current(p)->kind == TK_COLON) {
                    expect(
                        p, 
                        TK_SEMICOLON, 
                        "field cannot be declared in a block"
                    );
                } else {
                    expect_semicolon(p);
                }
            }
            child = astnode_exprstmt_new(p, n);
        }

        if (child) bufpush(ast, child);
    }
    return astnode_block_new(
        p, 
        lbrace,
        ast,
        value,
        p->token_idx-1
    );
}

void parse(ParseCtx* p) {
    while (current(p)->kind != TK_EOF) {
        Astnode* astnode = parse_astnode_root(p);
        if (astnode) bufpush(p->src->ast, astnode);
    }
}
