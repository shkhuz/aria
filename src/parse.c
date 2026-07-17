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

#define current(p) (tk(p->src, p->current))
#define currentspan(p) (tkspan(p->src, p->current))
#define prev(p) (tk(p->src, p->current-1))
#define prevspan(p) (tkspan(p->src, p->current-1))

static inline Span tkspan(Srcfile* src, TokenIndex idx) {
    return listget(src->tokens, idx).span;
}

static inline Span ndspan(Srcfile* src, NodeIndex idx) {
    return listget(src->nodes, idx).span;
}

ParseCtx parsectx_new(
    struct Srcfile* src,
    struct CompileCtx* compilectx, 
    jmp_buf* error_handler_pos
) {
    ParseCtx p = (ParseCtx){};
    p.src = src;
    listinit(&compilectx->parsearena, p.src->nodes);
    listinit(&compilectx->parsearena, p.src->nextra);
    // Index 0 is a placeholder node.
    // Used to signify index 0 as empty/error.
    listpush(p.src->nodes, (Node){});
    // Index 1 is used for root node.
    // lhs & rhs are filled in at the end of parsing.
    listpush(p.src->nodes, (Node){
        AST_ROOT,
        (Span){},
        0,
        0
    });
    // Index 0 is error/empty token.
    p.current = 1;
    p.compilectx = compilectx;
    p.error = false;
    p.error_handler_pos = error_handler_pos;
    listinit(&compilectx->parsearena, p.sextra);
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
    if (p->current < listlen(p->src->tokens)) {
        p->current++;
    }
}

static void goto_prev_token(ParseCtx* p) {
    if (p->current > 0) {
        p->current--;
    }
}

static void check_eof(ParseCtx* p, TokenIndex pair) {
    if (current(p)->kind == TK_EOF) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "unexpected end of file",
            tkspan(p->src, pair)
        );
        msg_addl_fat(&msg, "while searching for:", tkspan(p->src, pair));
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
            currentspan(p)
        );
        // if (kind == TK_IDENT) {
        //     for (int i = 0; i < KEYWORDS_LEN; i++) {
        //         if (token_lexeme_eqlto(current(p), p->src, keywords[i].k)) {
        //             msg_addl_thin(
        //                 &msg, 
        //                 format_string("`%s` is a keyword", keywords[i].k)
        //             );
        //             break;
        //         }
        //     }
        // }
        msg_emit(p, &msg);
        return -1;
    }
    return p->current-1;
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
    if (!p->sextra) return;
    for (usize i = 0; i < listlen(p->sextra)-marker; i++) {
        listpush(p->src->nextra, listget(p->sextra, marker + i));
    }
    _listhdr(p->sextra)->len = marker;
}

static NodeIndex parse_atom_expr(ParseCtx* p) {
    if (match(p, TK_IDENT)) {
        listpush(p->src->nodes, (Node){
            AST_SYM,
            prevspan(p),
            p->current-1,
            0
        });
        return listlastidx(p->src->nodes);
    } else if (match(p, TK_KW_COMP)) {
        TokenIndex keyword = p->current-1;
        NodeIndex child = parse_atom_expr(p);
        listpush(p->src->nodes, (Node){
            AST_COMP,
            span_from_two(
                tkspan(p->src, keyword), 
                ndspan(p->src, child)
            ),
            child,
            0
        });
        return listlastidx(p->src->nodes);
    } else if (match(p, TK_KW_STRUCT)) {
        TokenIndex keyword = p->current-1;
        if (match(p, TK_LPAREN)) {
            TokenIndex path = expect(
                p, 
                TK_STRLIT, 
                "expected a string literal path"
            );
            TokenIndex rparen = expect_rparen(p);

            if (token_lexeme_eqlto(path, p->src, "\"\"")) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "empty path",
                    tkspan(p->src, path)
                );
                msg_emit_non_fatal(p, &msg);
                return 0;
            }

            strislice data = stri_lookup(
                &p->compilectx->interner, 
                tk(p->src, path)->extra
            );
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
                data.ptr,
                data.len
            );
            path_wc[start + data.len] = '\0';
            int src = read_srcfile(
                p->compilectx,
                path_wc,
                tkspan(p->src, path),
                p->src
            );
            listpush(p->src->nodes, (Node){
                AST_IMPORT,
                span_from_two(
                    tkspan(p->src, keyword), 
                    tkspan(p->src, rparen)
                ),
                src,
                path
            });
            return listlastidx(p->src->nodes);
        } else if (match(p, TK_LBRACE)) {
            TokenIndex lbrace = p->current-1;
            usize marker = listlen(p->sextra);
            while (!match(p, TK_RBRACE)) {
                check_eof(p, lbrace);
                NodeIndex child = parse_astnode_root(p);
                listpush(p->sextra, child);
            }

            listpush(p->src->nodes, (Node){
                AST_STRUCT,
                span_from_two(
                    tkspan(p->src, keyword), 
                    prevspan(p)
                ),
                listlen(p->src->nextra),
                listlen(p->sextra) - marker
            });
            flush_sextra(p, marker);

            return listlastidx(p->src->nodes);
        }
    } else if (current(p)->kind == TK_LBRACE) {
        return parse_block(p);
    }

    Msg msg = msg_with_span(
        MSG_ERROR,
        current(p)->kind == TK_SEMICOLON
            ? "unexpected `;`"
            : "expected expression",
        currentspan(p)
    );
    msg_emit(p, &msg);
    return 0;
}

static NodeIndex parse_vardecl(ParseCtx* p) {
    TokenIndex keyword = p->current-1;
    bool imm = true;
    if (tk(p->src, keyword)->kind == TK_KW_MUT) imm = false;
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
            tkspan(p->src, ident)
        );
        msg_emit(p, &msg);
    }

    listpush(p->src->nodes, (Node){
        AST_VARDECL,
        span_from_two(
            tkspan(p->src, keyword), 
            ndspan(p->src, init == 0 ? type : init)
        ),
        listlen(p->src->nextra),
        ident
    });
    listpush(p->src->nextra, keyword);
    listpush(p->src->nextra, type);
    listpush(p->src->nextra, init);
    return listlastidx(p->src->nodes);
}

static NodeIndex parse_func(ParseCtx* p) {
    TokenIndex keyword = p->current-1;
    TokenIndex ident = expect(p, TK_IDENT, "expected function name");
    TokenIndex lparen = expect_lparen(p);

    usize marker = listlen(p->sextra);
    while (!match(p, TK_RPAREN)) {
        check_eof(p, lparen);
        TokenIndex pident = expect(
            p, 
            TK_IDENT, 
            "expected parameter name"
        );
        expect_colon(p);
        NodeIndex ptype = parse_atom_expr(p);
        listpush(p->src->nodes, (Node){
            AST_PARAM,
            span_from_two(
                tkspan(p->src, pident), 
                ndspan(p->src, ptype)
            ),
            pident,
            ptype,
        });
        listpush(p->sextra, listlastidx(p->src->nodes));
        if (current(p)->kind != TK_RPAREN) {
            expect_comma(p);
        }
    }

    NodeIndex returntype = parse_atom_expr(p);
    if (current(p)->kind != TK_LBRACE 
            && nd(p->src, returntype)->kind == AST_BLOCK) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "expected `{` for function body",
            currentspan(p)
        );
        msg_addl_fat(
            &msg, 
            "perhaps you forgot the return type?", 
            span_only_firstchar(nd(p->src, returntype)->span)
        );
        msg_emit(p, &msg);
    }

    NodeIndex body = parse_block(p);
    listpush(p->src->nodes, (Node){
        AST_FNDECL,
        span_from_two(tkspan(p->src, keyword), prevspan(p)),
        listlen(p->src->nextra),
        ident 
    });
    listpush(p->src->nextra, listlen(p->sextra)-marker);
    listpush(p->src->nextra, returntype);
    listpush(p->src->nextra, body);
    flush_sextra(p, marker);
    return listlastidx(p->src->nodes);
}

static NodeIndex parse_astnode_root(ParseCtx* p) {
    if (match(p, TK_KW_IMM) || match(p, TK_KW_MUT)) {
        return parse_vardecl(p);
    } else if (match(p, TK_KW_FN)) {
        return parse_func(p); 
    } else if (match(p, TK_IDENT)) {
        TokenIndex ident = p->current-1;
        if (match(p, TK_COLON)) {
            NodeIndex type = parse_atom_expr(p);
            if (current(p)->kind != TK_RBRACE) {
                expect_comma(p);
            }

            listpush(p->src->nodes, (Node){
                AST_FIELD,
                span_from_two(
                    tkspan(p->src, ident), 
                    ndspan(p->src, type)
                ),
                ident,
                type
            });
            return listlastidx(p->src->nodes);
        } else {
            Msg msg = msg_with_span(
                MSG_ERROR,
                "expected `:` for field declaration",
                currentspan(p)
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
            currentspan(p)
        );
        msg_emit(p, &msg);
    }
    return 0;
}

static NodeIndex parse_block(ParseCtx* p) {
    TokenIndex lbrace = expect_lbrace(p);
    usize marker = listlen(p->sextra);
    NodeIndex value = 0;

    while (!match(p, TK_RBRACE)) {
        check_eof(p, lbrace);
        NodeIndex child = 0;
        if (match(p, TK_KW_IMM) || match(p, TK_KW_MUT)) {
            child = parse_vardecl(p); 
        } else if (match(p, TK_KW_FN)) {
            child = parse_func(p);
        } else if (match(p, TK_KW_YIELD)) {
            value = parse_atom_expr(p);
            expect_semicolon(p);
            if (!match(p, TK_RBRACE)) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "expected `}`",
                    currentspan(p)
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
            Node* pn = nd(p->src, n);
            if (pn->kind == AST_BLOCK
                || (pn->kind == AST_COMP 
                    && listget(p->src->nodes, pn->lhs).kind == AST_BLOCK)
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

            listpush(p->src->nodes, (Node){
                AST_EXPRSTMT,
                ndspan(p->src, n),
                n,
                0
            });
            child = listlastidx(p->src->nodes);
        }

        listpush(p->sextra, child);
    }

    listpush(p->src->nodes, (Node){
        AST_BLOCK,
        span_from_two(tkspan(p->src, lbrace), prevspan(p)),
        listlen(p->src->nextra),
        0
    });
    listpush(p->src->nextra, listlen(p->sextra)-marker);
    listpush(p->src->nextra, value);
    flush_sextra(p, marker);
    return listlastidx(p->src->nodes);
}

void parse(ParseCtx* p) {
    usize marker = listlen(p->sextra);
    while (current(p)->kind != TK_EOF) {
        NodeIndex child = parse_astnode_root(p);
        listpush(p->sextra, child);
    }

    Node* root = nd(p->src, 1);
    root->lhs = listlen(p->src->nextra);
    root->rhs = listlen(p->sextra) - marker;
    flush_sextra(p, marker);
}
