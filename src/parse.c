#include "parse.h"
#include "srcfile.h"
#include "msg.h"
#include "token.h"
#include "node.h"
#include "compile.h"

static NodeIndex parse_astnode_root(ParseCtx* p);
static NodeIndex parse_block(ParseCtx* p);

#define msg_with_span(kind, msg, span) _msg_with_span(kind, msg, span, p->src)
#define msg_addl_fat(m, msg, span) _msg_addl_fat(m, msg, span, p->src)

static inline Span tkspan(ParseCtx* p, TokenIndex idx) {
    return p->src->tokens[idx].span;
}

static inline Span ndspan(ParseCtx* p, NodeIndex idx) {
    return p->src->nodes[idx].span;
}

static inline Token* tk(ParseCtx* p, TokenIndex idx) {
    return &p->src->tokens[idx];
}

static inline Node* nd(ParseCtx* p, NodeIndex idx) {
    return &p->src->nodes[idx];
}

static inline Token* current(ParseCtx* p) {
    return tk(p, p->token_idx);
}

static inline Token* prev(ParseCtx* p) {
    if (p->token_idx > 0) return tk(p, p->token_idx - 1);
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
    p.src->nodes = NULL;
    p.src->nextra = NULL;
    // Index 0 is a placeholder node.
    // Used to signify index 0 as empty/error.
    bufpush(p.src->nodes, (Node){});
    // Index 1 is used for root node.
    // lhs & rhs are filled in at the end of parsing.
    bufpush(p.src->nodes, (Node){
        AST_ROOT,
        (Span){},
        0,
        0
    });
    // Index 0 is error/empty token.
    p.token_idx = 1;
    p.compilectx = compilectx;
    p.error = false;
    p.error_handler_pos = error_handler_pos;
    p.sextra = NULL;
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
        msg_addl_fat(&msg, "while searching for:", tk(p, pair)->span);
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

static void flush_sextra(ParseCtx* p, usize marker) {
    for (usize i = 0; i < buflen(p->sextra)-marker; i++) {
        bufpush(p->src->nextra, p->sextra[marker + i]);
    }
    _bufhdr(p->sextra)->len = marker;
}

static NodeIndex parse_atom_expr(ParseCtx* p) {
    if (match(p, TK_IDENT)) {
        bufpush(p->src->nodes, (Node){
            AST_SYM,
            prev(p)->span,
            p->token_idx-1,
            0
        });
        return buflastidx(p->src->nodes);
    } else if (match(p, TK_KW_COMP)) {
        TokenIndex keyword = p->token_idx-1;
        NodeIndex child = parse_atom_expr(p);
        bufpush(p->src->nodes, (Node){
            AST_COMP,
            span_from_two(tkspan(p, keyword), ndspan(p, child)),
            child,
            0
        });
        return buflastidx(p->src->nodes);
    } else if (match(p, TK_KW_STRUCT)) {
        TokenIndex keyword = p->token_idx-1;
        if (match(p, TK_LPAREN)) {
            TokenIndex path = expect(
                p, 
                TK_STRLIT, 
                "expected a string literal path"
            );
            TokenIndex rparen = expect_rparen(p);

            if (token_lexeme_eqlto(tk(p, path), p->src, "\"\"")) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "empty path",
                    tkspan(p, path)
                );
                msg_emit_non_fatal(p, &msg);
                return 0;
            }

            StrlitData* data = &token_strlit_data[tk(p, path)->extra];
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
            int src = read_srcfile(
                p->compilectx,
                path_wc,
                tkspan(p, path),
                p->src
            );
            bufpush(p->src->nodes, (Node){
                AST_IMPORT,
                span_from_two(tkspan(p, keyword), tkspan(p, rparen)),
                src,
                path
            });
            return buflastidx(p->src->nodes);
        } else if (match(p, TK_LBRACE)) {
            TokenIndex lbrace = p->token_idx-1;
            usize marker = buflen(p->sextra);
            while (!match(p, TK_RBRACE)) {
                check_eof(p, lbrace);
                NodeIndex child = parse_astnode_root(p);
                bufpush(p->sextra, child);
            }

            bufpush(p->src->nodes, (Node){
                AST_STRUCT,
                span_from_two(tkspan(p, keyword), prev(p)->span),
                buflen(p->src->nextra),
                buflen(p->sextra) - marker
            });
            flush_sextra(p, marker);

            return buflastidx(p->src->nodes);
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
    return 0;
}

static NodeIndex parse_vardecl(ParseCtx* p) {
    TokenIndex keyword = p->token_idx-1;
    bool imm = true;
    if (tk(p, keyword)->kind == TK_KW_MUT) imm = false;
    TokenIndex ident = expect(p, TK_IDENT, "expected variable name");
    NodeIndex type = 0;
    if (match(p, TK_COLON)) {
        type = parse_atom_expr(p);
    }
    NodeIndex init = 0;
    if (match(p, TK_EQUAL)) {
        init = parse_atom_expr(p);
    }
    expect_semicolon(p);
    if (type == 0 && init == 0) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "variable without type or initializer",
            tkspan(p, ident)
        );
        msg_emit(p, &msg);
    }

    bufpush(p->src->nodes, (Node){
        AST_VARDECL,
        span_from_two(
            tkspan(p, keyword), 
            ndspan(p, init == 0 ? type : init)
        ),
        buflen(p->src->nextra),
        ident
    });
    bufpush(p->src->nextra, keyword);
    bufpush(p->src->nextra, type);
    bufpush(p->src->nextra, init);
    return buflastidx(p->src->nodes);
}

static NodeIndex parse_func(ParseCtx* p) {
    TokenIndex keyword = p->token_idx-1;
    TokenIndex ident = expect(p, TK_IDENT, "expected function name");
    TokenIndex lparen = expect_lparen(p);

    usize marker = buflen(p->sextra);
    while (!match(p, TK_RPAREN)) {
        check_eof(p, lparen);
        TokenIndex pident = expect(
            p, 
            TK_IDENT, 
            "expected parameter name"
        );
        expect_colon(p);
        NodeIndex ptype = parse_atom_expr(p);
        bufpush(p->src->nodes, (Node){
            AST_PARAM,
            span_from_two(tkspan(p, pident), ndspan(p, ptype)),
            pident,
            ptype,
        });
        bufpush(p->sextra, buflastidx(p->src->nodes));
        if (current(p)->kind != TK_RPAREN) {
            expect_comma(p);
        }
    }

    NodeIndex returntype = parse_atom_expr(p);
    if (current(p)->kind != TK_LBRACE && nd(p, returntype)->kind == AST_BLOCK) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "expected `{` for function body",
            current(p)->span
        );
        msg_addl_fat(
            &msg, 
            "perhaps you forgot the return type?", 
            span_only_firstchar(nd(p, returntype)->span)
        );
        msg_emit(p, &msg);
    }

    NodeIndex body = parse_block(p);
    bufpush(p->src->nodes, (Node){
        AST_FNDECL,
        span_from_two(tkspan(p, keyword), prev(p)->span),
        buflen(p->src->nextra),
        ident 
    });
    bufpush(p->src->nextra, buflen(p->sextra)-marker);
    bufpush(p->src->nextra, returntype);
    bufpush(p->src->nextra, body);
    flush_sextra(p, marker);
    return buflastidx(p->src->nodes);
}

static NodeIndex parse_astnode_root(ParseCtx* p) {
    if (match(p, TK_KW_IMM) || match(p, TK_KW_MUT)) {
        return parse_vardecl(p);
    } else if (match(p, TK_KW_FUN)) {
        return parse_func(p); 
    } else if (match(p, TK_IDENT)) {
        TokenIndex ident = p->token_idx-1;
        if (match(p, TK_COLON)) {
            NodeIndex type = parse_atom_expr(p);
            if (current(p)->kind != TK_RBRACE) {
                expect_comma(p);
            }

            bufpush(p->src->nodes, (Node){
                AST_FIELD,
                span_from_two(tkspan(p, ident), ndspan(p, type)),
                ident,
                type
            });
            return buflastidx(p->src->nodes);
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
    return 0;
}

static NodeIndex parse_block(ParseCtx* p) {
    TokenIndex lbrace = expect_lbrace(p);
    usize marker = buflen(p->sextra);
    NodeIndex value = 0;

    while (!match(p, TK_RBRACE)) {
        check_eof(p, lbrace);
        NodeIndex child = 0;
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
            NodeIndex n = parse_atom_expr(p);
            // The type of AST that warrants skipping the semicolon 
            // should have a child of kind AST_BLOCK or itself be AST_BLOCK.
            // Nodes having child at the end:
            // - AST_COMP
            // - AST_IF
            // - ...
            Node* pn = nd(p, n);
            if (pn->kind == AST_BLOCK
                || (pn->kind == AST_COMP 
                    && p->src->nodes[pn->lhs].kind == AST_BLOCK)
            ) {
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

            bufpush(p->src->nodes, (Node){
                AST_EXPRSTMT,
                ndspan(p, n),
                n,
                0
            });
            child = buflastidx(p->src->nodes);
        }

        bufpush(p->sextra, child);
    }

    bufpush(p->src->nodes, (Node){
        AST_BLOCK,
        span_from_two(tkspan(p, lbrace), prev(p)->span),
        buflen(p->src->nextra),
        0
    });
    bufpush(p->src->nextra, buflen(p->sextra)-marker);
    bufpush(p->src->nextra, value);
    flush_sextra(p, marker);
    return buflastidx(p->src->nodes);
}

void parse(ParseCtx* p) {
    usize marker = buflen(p->sextra);
    while (current(p)->kind != TK_EOF) {
        NodeIndex child = parse_astnode_root(p);
        bufpush(p->sextra, child);
    }

    Node* root = nd(p, 1);
    root->lhs = buflen(p->src->nextra);
    root->rhs = buflen(p->sextra) - marker;
    flush_sextra(p, marker);
}
