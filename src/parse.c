#include "parse.h"
#include "srcfile.h"
#include "msg.h"
#include "token.h"
#include "ast.h"
#include "compile.h"

static Astnode* parse_astnode_root(ParseCtx* p);
static Astnode* parse_block(ParseCtx* p);

#define msg_with_span(kind, msg, span) _msg_with_span(kind, msg, span, p->src)
#define msg_addl_fat(m, msg, span) _msg_addl_fat(m, msg, span, p->src)

ParseCtx parsectx_new(
    struct Srcfile* src,
    struct CompileCtx* compilectx, 
    jmp_buf* error_handler_pos
) {
    ParseCtx p;
    p.src = src;
    p.src->ast = NULL;
    p.current = p.src->tokens[0];
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
    if (p->token_idx < buflen(p->src->tokens)) {
        p->token_idx++;
        p->prev = p->current;
        p->current = p->src->tokens[p->token_idx];
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
                if (token_lexeme_eqlto(p->current, p->src, keywords[i].k)) {
                    msg_addl_thin(
                        &msg, 
                        format_string("`%s` is a keyword", keywords[i].k)
                    );
                    break;
                }
            }
        }
        msg_emit(p, &msg);
        return NULL;
    }
    return p->prev;
}

static Token* expect_lparen(ParseCtx* p) {
    return expect(p, TK_LPAREN, "expected `(`");
}

static Token* expect_rparen(ParseCtx* p) {
    return expect(p, TK_RPAREN, "expected `)`");
}

static Token* expect_lbrace(ParseCtx* p) {
    return expect(p, TK_LBRACE, "expected `{`");
}

static inline Token* expect_colon(ParseCtx* p) {
    return expect(p, TK_COLON, "expected `:`");
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
    } else if (match(p, TK_KW_COMP)) {
        Token* keyword = p->prev;
        Astnode* child = parse_atom_expr(p);
        return astnode_comp_new(keyword, child);
    } else if (match(p, TK_KW_STRUCT)) {
        Token* keyword = p->prev;
        if (match(p, TK_LPAREN)) {
            Token* path = expect(
                p, 
                TK_STRLIT, 
                "expected a string literal path"
            );
            expect_rparen(p);

            if (token_lexeme_eqlto(path, p->src, "\"\"")) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "empty path",
                    path->span
                );
                msg_emit_non_fatal(p, &msg);
                return NULL;
            }

            StrlitData* data = &token_strlit_data[path->extra];
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
                path->span,
                p->src
            );
            return astnode_struct_import_new(
                keyword, 
                path, 
                p->prev, 
                src
            );
        } else if (match(p, TK_LBRACE)) {
            Token* lbrace = p->prev;
            Astnode** ast = NULL;
            while (!match(p, TK_RBRACE)) {
                check_eof(p, lbrace);
                Astnode* n = parse_astnode_root(p);
                if (n) bufpush(ast, n);
            }
            return astnode_struct_inline_new(
                keyword,
                ast,
                p->prev
            );
        }
    } else if (p->current->kind == TK_LBRACE) {
        return parse_block(p);
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
        keyword, 
        ident,
        type,
        NULL,
        initializer
    );
}

static Astnode* parse_func(ParseCtx* p) {
    Token* keyword = p->prev;
    Token* ident = expect(p, TK_IDENT, "expected function name");
    Token* lparen = expect_lparen(p);
    Astnode** params = NULL;
    while (!match(p, TK_RPAREN)) {
        check_eof(p, lparen);
        Token* pident = expect(
            p, 
            TK_IDENT, 
            "expected parameter name"
        );
        expect_colon(p);
        Astnode* ptype = parse_atom_expr(p);
        bufpush(params, astnode_param_new(pident, ptype));
        if (p->current->kind != TK_RPAREN) {
            expect_comma(p);
        }
    }
    Astnode* returntype = parse_atom_expr(p);
    if (p->current->kind != TK_LBRACE && returntype->kind == AST_BLOCK) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "expected `{` for function body",
            p->current->span
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
        Token* ident = p->prev;
        if (match(p, TK_COLON)) {
            Astnode* type = parse_atom_expr(p);
            if (p->current->kind != TK_RBRACE) {
                expect_comma(p);
            }
            return astnode_field_new(ident, type);
        } else {
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
    } else {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "expected top-level declaration",
            p->current->span
        );
        msg_emit(p, &msg);
    }
}

static Astnode* parse_block(ParseCtx* p) {
    Token* lbrace = expect_lbrace(p);
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
                    p->current->span
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
                if (p->current->kind == TK_COLON) {
                    expect(
                        p, 
                        TK_SEMICOLON, 
                        "field cannot be declared in a block"
                    );
                } else {
                    expect_semicolon(p);
                }
            }
            child = astnode_exprstmt_new(n);
        }

        if (child) bufpush(ast, child);
    }
    return astnode_block_new(
        lbrace,
        ast,
        value,
        p->prev
    );
}

void parse(ParseCtx* p) {
    while (p->current->kind != TK_EOF) {
        Astnode* astnode = parse_astnode_root(p);
        if (astnode) bufpush(p->src->ast, astnode);
    }
}
