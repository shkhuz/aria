#include "parse.h"
#include "buf.h"
#include "msg.h"
#include "compile.h"

static AstNode* parse_root(ParseCtx* p, bool error_on_no_match);
static AstNode* parse_expr(ParseCtx* p);
static AstNode* parse_scoped_block(ParseCtx* p);
static AstNode* parse_typespec(ParseCtx* p);

ParseCtx parse_new_context(
    Srcfile* srcfile,
    CompileCtx* compile_ctx,
    jmp_buf* error_handler_pos)
{
    ParseCtx p;
    p.srcfile = srcfile;
    p.srcfile->astnodes = NULL;
    p.token_idx = 0;
    p.current = p.srcfile->tokens[0];
    p.prev = NULL;
    p.compile_ctx = compile_ctx;
    p.error = false;
    p.error_handler_pos = error_handler_pos;
    return p;
}

static inline void msg_emit(ParseCtx* p, Msg* msg) {
    _msg_emit(msg, p->compile_ctx);
    if (msg->kind == MSG_ERROR) {
        p->error = true;
        longjmp(*p->error_handler_pos, 1);
    }
}

static inline void msg_emit_non_fatal(ParseCtx* p, Msg* msg) {
    _msg_emit(msg, p->compile_ctx);
    if (msg->kind == MSG_ERROR) {
        p->error = true;
    }
}

static void goto_next_tok(ParseCtx* p) {
    if (p->token_idx < buflen(p->srcfile->tokens)) {
        p->token_idx++;
        p->prev = p->current;
        p->current = p->srcfile->tokens[p->token_idx];
    }
}

//static void goto_prev_tok(ParseCtx* p) {
//    if (p->token_idx > 0) {
//        p->token_idx--;
//    }
//}

static void check_eof(ParseCtx* p, Token* pair) {
    if (p->current->kind == TOKEN_EOF) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "unexpected end of file",
            p->current->span);
        msg_addl_fat(&msg, "while matching pair of:", pair->span);
        msg_emit(p, &msg);
    }
}

static bool match(ParseCtx* p, TokenKind kind) {
    if (p->current->kind == kind) {
        goto_next_tok(p);
        return true;
    }
    return false;
}

static Token* expect(ParseCtx* p, TokenKind kind, const char* msgstr) {
    if (!match(p, kind)) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            msgstr,
            p->current->span);
        if (kind == TOKEN_IDENTIFIER) {
            bufloop(keywords, i) {
                if (is_token_lexeme(p->current, keywords[i].k)) {
                    msg_addl_thin(&msg, format_string("`%s` is a keyword", keywords[i].k));
                }
            }
        }
        msg_emit(p, &msg);
        return NULL;
    }
    return p->prev;
}

static inline Token* expect_identifier(ParseCtx* p, const char* msg) {
    return expect(p, TOKEN_IDENTIFIER, msg);
}

static inline Token* expect_lparen(ParseCtx* p) {
    return expect(p, TOKEN_LPAREN, "expected `(`");
}

static inline Token* expect_rparen(ParseCtx* p) {
    return expect(p, TOKEN_RPAREN, "expected `)`");
}

static inline Token* expect_lbrace(ParseCtx* p) {
    return expect(p, TOKEN_LBRACE, "expected `{`");
}

static inline Token* expect_rbrack(ParseCtx* p) {
    return expect(p, TOKEN_RBRACK, "expected `]`");
}

static inline Token* expect_equal(ParseCtx* p) {
    return expect(p, TOKEN_EQUAL, "expected `=`");
}

static inline Token* expect_colon(ParseCtx* p) {
    return expect(p, TOKEN_COLON, "expected `:`");
}

static inline Token* expect_semicolon(ParseCtx* p) {
    return expect(p, TOKEN_SEMICOLON, "expected `;`");
}

static inline Token* expect_comma(ParseCtx* p) {
    return expect(p, TOKEN_COMMA, "expected `,`");
}

static inline Token* expect_dot(ParseCtx* p) {
    return expect(p, TOKEN_DOT, "expected `.`");
}

static inline Token* expect_langbr(ParseCtx* p) {
    return expect(p, TOKEN_LANGBR, "expected `<`");
}

static AstNode** parse_args(
        ParseCtx* p,
        Token* opening,
        TokenKind closing,
        bool expr,
        bool at_least_one) {
    AstNode** args = NULL;
    while (p->current->kind != closing || at_least_one) {
        check_eof(p, opening);
        at_least_one = false;
        AstNode* arg = NULL;
        if (expr) {
            arg = parse_expr(p);
        } else {
            arg = parse_typespec(p);
        }
        bufpush(args, arg);
        if (p->current->kind != closing) {
            expect(
                p,
                TOKEN_COMMA,
                format_string("expected `,` or `%s`", tokenkind_to_string(closing)));
        }
    }
    expect(p, closing, format_string("expected `%s`", tokenkind_to_string(closing)));
    return args;
}

static AstNode* parse_generic_typespec(ParseCtx* p, AstNode* left, bool expr) {
    if (match(p, TOKEN_DOUBLE_COLON) && !expr) {
        Msg msg = msg_with_span(
            MSG_WARNING,
            "no need to prepend `::` when already in type ctx",
            p->prev->span);
        msg_emit_non_fatal(p, &msg);
    }
    Token* langbr = expect_langbr(p);
    AstNode** args = parse_args(p, langbr, TOKEN_RANGBR, false, true);
    return astnode_generic_typespec_new(left, args, p->prev);
}

static AstNode* parse_access_expr(ParseCtx* p, AstNode* left, bool expr) {
    AstNode* right = astnode_symbol_new(expect_identifier(p, "expected symbol name"));
    AstNode* access = astnode_access_new(left, right);
    if ((p->current->kind == TOKEN_LANGBR && !expr) || p->current->kind == TOKEN_DOUBLE_COLON) {
        access = parse_generic_typespec(p, access, expr);
    }
    return access;
}

static AstNode* parse_directive(ParseCtx* p) {
    Token* start = p->prev;
    Token* identifier = expect_identifier(p, "expected directive name");
    DirectiveKind kind = DIRECTIVE_NONE;
    bufloop(directives, i) {
        if (slice_eql_to_str(
                &identifier->span.srcfile->handle.contents[identifier->span.start],
                identifier->span.end - identifier->span.start,
                directives[i].k)) {
            kind = directives[i].v;
            break;
        }
    }

    if (kind == DIRECTIVE_NONE) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "unknown directive",
            identifier->span);
        msg_emit_non_fatal(p, &msg);
    }

    Token* lparen = expect_lparen(p);
    AstNode** args = parse_args(p, lparen, TOKEN_RPAREN, true, false);
    return astnode_directive_new(start, identifier, args, kind, p->prev);
}

static AstNode* parse_atom_typespec(ParseCtx* p) {
    if (match(p, TOKEN_AT)) {
        return parse_directive(p);
    } else if (match(p, TOKEN_IDENTIFIER)) {
        AstNode* left = astnode_symbol_new(p->prev);
        if (p->current->kind == TOKEN_DOUBLE_COLON || p->current->kind == TOKEN_LANGBR) {
            left = parse_generic_typespec(p, left, false);
        }

        while (match(p, TOKEN_DOT)) {
            left = parse_access_expr(p, left, false);
        }
        return left;
    } else if (match(p, TOKEN_STAR)) {
        Token* star = p->prev;
        bool immutable = false;
        if (match(p, TOKEN_KEYWORD_IMM)) {
            immutable = true;
        }
        AstNode* child = parse_typespec(p);
        return astnode_typespec_ptr_new(star, immutable, child);
    } else if (match(p, TOKEN_LBRACK)) {
        Token* lbrack = p->prev;
        if (match(p, TOKEN_RBRACK)) {
            bool immutable = false;
            if (match(p, TOKEN_KEYWORD_IMM)) {
                immutable = true;
            }
            AstNode* child = parse_typespec(p);
            return astnode_typespec_slice_new(lbrack, immutable, child);
        } else {
            AstNode* size = parse_expr(p);
            expect_rbrack(p);
            AstNode* child = parse_typespec(p);
            return astnode_typespec_array_new(lbrack, size, child);
        }
    } else if (match(p, TOKEN_LPAREN)) {
        Token* lparen = p->prev;
        AstNode** elems = parse_args(p, lparen, TOKEN_RPAREN, false, false);
        return astnode_typespec_tuple_new(lparen, elems, p->prev);
    } else if (match(p, TOKEN_KEYWORD_FN)) {
        Token* start = p->prev;
        Token* lparen = expect_lparen(p);
        AstNode** params = parse_args(p, lparen, TOKEN_RPAREN, true, false);
        AstNode* ret_typespec = parse_typespec(p);
        return astnode_typespec_func_new(start, params, ret_typespec);
    }
    // NOTE: Add the case to `can_token_start_typespec()`

    Msg msg = msg_with_span(
        MSG_ERROR,
        "expected type",
        p->current->span);
    msg_emit(p, &msg);
    return NULL;
}

static AstNode* parse_typespec(ParseCtx* p) {
    return parse_atom_typespec(p);
}

static AstNode* parse_if_branch(ParseCtx* p, Token* keyword, IfBranchKind kind) {
    AstNode* cond = NULL;

    if (kind != IFBR_ELSE) {
        expect_lparen(p);
        cond = parse_expr(p);
        expect_rparen(p);
    }

    AstNode* body = parse_expr(p);
    if (body->kind == ASTNODE_IF) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "`if` branch may not contain another `if`",
            body->span);
        msg_addl_thin(&msg, "enclose body with `{}`");
        msg_emit(p, &msg);
    }
    return astnode_if_branch_new(keyword, kind, cond, body);
}

static inline AstNode* get_last_if_branch(
    AstNode* ifbr,
    AstNode** elseifbr,
    AstNode* elsebr)
{
    if (elsebr) return elsebr;
    else if (elseifbr) return *buflast(elseifbr);
    return ifbr;
}

static AstNode* parse_atom_expr(ParseCtx* p) {
    // NOTE: Add the case to `can_token_begin_expr()`
    if (match(p, TOKEN_IDENTIFIER)) {
        AstNode* left = astnode_symbol_new(p->prev);
        if (p->current->kind == TOKEN_DOUBLE_COLON) {
            left = parse_generic_typespec(p, left, true);
        }

        while (match(p, TOKEN_DOT)) {
            left = parse_access_expr(p, left, true);
        }

        if (match(p, TOKEN_LBRACE)) {
            Token* lbrace = p->prev;
            AstNode** fields = NULL;
            while (!match(p, TOKEN_RBRACE)) {
                check_eof(p, lbrace);
                Token* dot = expect_dot(p);
                Token* field_name = expect_identifier(p, "expected field name");
                expect_equal(p);
                AstNode* field_value = parse_expr(p);
                if (p->current->kind != TOKEN_RBRACE) {
                    expect_comma(p);
                }
                bufpush(fields, astnode_field_in_literal_new(dot, field_name, field_value));
            }
            return astnode_aggregate_literal_new(left, fields, p->prev);
        } else {
            return left;
        }
    } else if (match(p, TOKEN_AT)) {
        return parse_directive(p);
    } else if (match(p, TOKEN_LBRACE)) {
        return parse_scoped_block(p);
    } else if (match(p, TOKEN_KEYWORD_IF)) {
        AstNode* ifbr = parse_if_branch(p, p->prev, IFBR_IF);
        AstNode** elseifbr = NULL;
        AstNode* elsebr = NULL;

        for (;;) {
            if (match(p, TOKEN_KEYWORD_ELSE)) {
                Token* keyword = p->prev;
                if (match(p, TOKEN_KEYWORD_IF)) {
                    bufpush(elseifbr, parse_if_branch(p, keyword, IFBR_ELSEIF));
                } else {
                    elsebr = parse_if_branch(p, keyword, IFBR_ELSE);
                    break;
                }
            }
            else break;
        }

        return astnode_if_new(
            ifbr,
            elseifbr,
            elsebr,
            get_last_if_branch(ifbr, elseifbr, elsebr));
    } else if (match(p, TOKEN_KEYWORD_RETURN)) {
        Token* keyword = p->prev;
        AstNode* operand = NULL;
        if (can_token_start_expr(p->current)) {
            operand = parse_expr(p);
        }
        return astnode_return_new(keyword, operand);
    } else if (match(p, TOKEN_INTEGER_LITERAL)) {
        Token* token = p->prev;
        bigint val;
        bigint_init(&val);
        // TODO: move so that these are only initialized once
        bigint base;
        bigint_init_u64(&base, 10);
        bigint digit;
        bigint_init(&digit);

        for (usize i = token->span.start; i < token->span.end; i++) {
            char c = token->span.srcfile->handle.contents[i];
            if (c != '_') {
                int d = c - '0';
                bigint_set_u64(&digit, (u64)d);
                bigint_mul(&val, &base, &val);
                bigint_add(&val, &digit, &val);
            }
        }

        return astnode_integer_literal_new(token, val);
    } else if (match(p, TOKEN_DOT)) {
        Token* start = p->prev;
        if (match(p, TOKEN_LPAREN)) {
            // tuple literal
            Token* lparen = p->prev;
            AstNode** elems = parse_args(p, lparen, TOKEN_RPAREN, true, false);
            return astnode_tuple_literal_new(start, elems, p->prev);
        } else {
            // enum literal
            expect_identifier(p, "expected enum variant name");
            assert(false && "Not implemented");
        }
    } else if (match(p, TOKEN_LBRACK)) {
        Token* lbrack = p->prev;
        AstNode** elems = parse_args(p, lbrack, TOKEN_RBRACK, true, false);
        // TODO: if we want to specify a type for the literal, we could check
        // for it after the `]`. But it would signify the child type, not the
        // array literal type.
        return astnode_array_literal_new(lbrack, elems, p->prev);
    } else if (match(p, TOKEN_LPAREN)) {
        Token* lparen = p->prev;
        AstNode* expr = parse_expr(p);
        expect_rparen(p);
        return expr;
    }
    // NOTE: Add the case to `can_token_begin_expr()`

    Msg msg = msg_with_span(
        MSG_ERROR,
        "expected expression",
        p->current->span);
    msg_emit(p, &msg);
    return NULL;
}

static AstNode* parse_suffix_expr(ParseCtx* p) {
    AstNode* left = parse_atom_expr(p);
    while (p->current->kind == TOKEN_LPAREN
           || p->current->kind == TOKEN_DOT) {
        if (match(p, TOKEN_LPAREN)) {
            Token* lparen = p->prev;
            AstNode** args = parse_args(p, lparen, TOKEN_RPAREN, true, false);
            left = astnode_function_call_new(left, args, p->prev);
        } else if (match(p, TOKEN_DOT)) {
            left = parse_access_expr(p, left, true);
        }
        // NOTE: also add above in the while cond in `parse_suffix_expr` above
    }
    return left;
}

static AstNode* parse_expr(ParseCtx* p) {
    return parse_suffix_expr(p);
}

static AstNode* parse_function_header(ParseCtx* p) {
    Token* keyword = p->prev;
    Token* identifier = expect_identifier(p, "expected function name");
    Token* lparen = expect_lparen(p);

    AstNode** params = NULL;
    while (!match(p, TOKEN_RPAREN)) {
        check_eof(p, lparen);
        Token* param_identifier =
            expect(p, TOKEN_IDENTIFIER, "expected parameter name or `)`");
        expect_colon(p);
        AstNode* param_type = parse_typespec(p);
        bufpush(params, astnode_param_new(param_identifier, param_type));
        if (p->current->kind != TOKEN_RPAREN) {
            expect(p, TOKEN_COMMA, "expected `,` or `)`");
        }
    }

    AstNode* ret_type = parse_typespec(p);
    return astnode_function_header_new(keyword, identifier, params, ret_type);
}

static AstNode* parse_scoped_block(ParseCtx* p) {
    Token* lbrace = p->prev;
    AstNode** stmts = NULL;
    AstNode* val = NULL;

    while (!match(p, TOKEN_RBRACE)) {
        check_eof(p, lbrace);
        AstNode* stmt = parse_root(p, false);
        if (stmt) bufpush(stmts, stmt);
        else {
            if (match(p, TOKEN_KEYWORD_YIELD)) {
                val = parse_expr(p);
                expect_semicolon(p);
                if (!match(p, TOKEN_RBRACE)) {
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        "expected `}`",
                        p->current->span);
                    msg_addl_thin(&msg, "`yield` must be last in a block");
                    msg_emit(p, &msg);
                }
                break;
            } else {
                AstNode* expr = parse_expr(p);
                if ((expr->kind == ASTNODE_IF
                     && get_last_if_branch(
                            expr->iff.ifbr,
                            expr->iff.elseifbr,
                            expr->iff.elsebr)->ifbr.body->kind == ASTNODE_SCOPED_BLOCK)
                    || expr->kind == ASTNODE_SCOPED_BLOCK) {
                } else {
                    expect_semicolon(p);
                }

                AstNode* exprstmt = astnode_exprstmt_new(expr);
                bufpush(stmts, exprstmt);
            }
        }
    }
    return astnode_scoped_block_new(lbrace, stmts, val, p->prev);
}

static AstNode* parse_root(ParseCtx* p, bool error_on_no_match) {
    if (match(p, TOKEN_KEYWORD_FN)) {
        AstNode* header = parse_function_header(p);
        expect_lbrace(p);
        AstNode* body = parse_scoped_block(p);
        return astnode_function_def_new(header, body);
    } else if (match(p, TOKEN_KEYWORD_STRUCT)) {
        Token* keyword = p->prev;
        Token* identifier = expect_identifier(p, "expected identifier");
        Token* lbrace = expect_lbrace(p);

        AstNode** fields = NULL;
        while (!match(p, TOKEN_RBRACE)) {
            check_eof(p, lbrace);
            if (match(p, TOKEN_IDENTIFIER)) {
                Token* field_identifier = p->prev;
                expect_colon(p);
                AstNode* field_typespec = parse_typespec(p);
                if (p->current->kind != TOKEN_RBRACE) {
                    expect_comma(p);
                }

                bool dup = false;
                bufloop(fields, i) {
                    if (are_token_lexemes_equal(fields[i]->field.key, field_identifier)) {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            "duplicate field",
                            field_identifier->span);
                        msg_addl_fat(&msg, "previous declaration here", fields[i]->field.key->span);
                        msg_emit_non_fatal(p, &msg);
                        dup = true;
                    }
                }
                if (!dup) bufpush(fields, astnode_field_new(field_identifier, field_typespec));
            } else {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "expected a field",
                    p->current->span);
                msg_emit(p, &msg);
            }
        }
        return astnode_struct_new(keyword, identifier, fields, p->prev);
    } else if (match(p, TOKEN_KEYWORD_IMPL)) {
        Token* keyword = p->prev;
        ImplKind implkind;
        AstNode* first = parse_typespec(p);
        AstNode* second = NULL;
        if (match(p, TOKEN_KEYWORD_FOR)) {
            implkind = IMPL_TRAIT;
            second = parse_typespec(p);
        }
        else implkind = IMPL_AGGREGATE;

        Token* lbrace = expect_lbrace(p);
        AstNode** children = NULL;
        while (!match(p, TOKEN_RBRACE)) {
            check_eof(p, lbrace);
            // For now, I just call `parse_root`, but this should
            // only accept function definitions and type aliases.
            AstNode* child = parse_root(p, true);
            bufpush(children, child);
        }
        return astnode_impl_new(
            keyword,
            implkind,
            implkind == IMPL_TRAIT ? first : NULL,
            implkind == IMPL_TRAIT ? second : first,
            children,
            p->prev);
    } else if (match(p, TOKEN_KEYWORD_IMM) || match(p, TOKEN_KEYWORD_MUT)) {
        Token* keyword = p->prev;
        bool immutable = true;
        if (keyword->kind == TOKEN_KEYWORD_MUT) immutable = false;
        Token* identifier = expect_identifier(p, "expected variable name");
        AstNode* type = NULL;
        if (match(p, TOKEN_COLON)) {
            type = parse_typespec(p);
        }
        expect_equal(p);
        AstNode* initializer = parse_expr(p);
        expect_semicolon(p);
        return astnode_variable_decl_new(
            keyword,
            immutable,
            identifier,
            type,
            initializer,
            p->prev);
    }

    if (error_on_no_match) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "expected a declaration or a definition",
            p->current->span);
        msg_emit(p, &msg);
    }
    return NULL;
}

void parse(ParseCtx* p) {
    while (p->current->kind != TOKEN_EOF) {
        AstNode* astnode = parse_root(p, true);
        if (astnode) bufpush(p->srcfile->astnodes, astnode);
    }
}
