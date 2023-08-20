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
    p.loop_breaks = NULL;
    p.loop_continues = NULL;
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
    Token* op = p->prev;
    AstNode* right = astnode_symbol_new(expect_identifier(p, "expected symbol name"));
    AstNode* access = astnode_access_new(op, left, right);
    if ((p->current->kind == TOKEN_LANGBR && !expr) || p->current->kind == TOKEN_DOUBLE_COLON) {
        access = parse_generic_typespec(p, access, expr);
    }
    return access;
}

static AstNode* parse_directive(ParseCtx* p) {
    Token* start = p->prev;
    Token* identifier = expect_identifier(p, "expected directive name");

    Token* lparen = expect_lparen(p);
    AstNode** args = parse_args(p, lparen, TOKEN_RPAREN, true, false);
    return astnode_directive_new(start, identifier, args, p->prev);
}

static AstNode* parse_identifier(ParseCtx* p, Token* identifier, bool expr) {
    AstNode* left = NULL;
    bufloop(builtin_symbols, i) {
        if (is_token_lexeme(identifier, builtin_symbols[i].k)) {
            left = astnode_builtin_symbol_new(builtin_symbols[i].v, identifier);
            break;
        }
    }
    if (!left) left = astnode_symbol_new(identifier);

    if (p->current->kind == TOKEN_DOUBLE_COLON || (!expr && p->current->kind == TOKEN_LANGBR)) {
        left = parse_generic_typespec(p, left, expr);
    }
    return left;
}

static AstNode* parse_atom_typespec(ParseCtx* p) {
    if (match(p, TOKEN_AT)) {
        return parse_directive(p);
    } else if (match(p, TOKEN_IDENTIFIER)) {
        return parse_identifier(p, p->prev, false);
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
        } else if (match(p, TOKEN_STAR)) {
            bool immutable = false;
            expect_rbrack(p);
            if (match(p, TOKEN_KEYWORD_IMM)) {
                immutable = true;
            }
            AstNode* child = parse_typespec(p);
            return astnode_typespec_multiptr_new(lbrack, immutable, child);
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

static AstNode* parse_suffix_typespec(ParseCtx* p) {
    AstNode* left = parse_atom_typespec(p);
    while (match(p, TOKEN_DOT)) {
        left = parse_access_expr(p, left, false);
    }
    return left;
}

static AstNode* parse_typespec(ParseCtx* p) {
    return parse_suffix_typespec(p);
}

static void parse_check_body_for_elsetail_expr(ParseCtx* p, AstNode* check, AstNodeKind parent_kind) {
    if (check->kind == ASTNODE_IF
        || check->kind == ASTNODE_WHILE
        /*|| check->kind == ASTNODE_FOR*/) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            format_string(
                "%s cannot contain another `if`, `while`, `for`",
                parent_kind == ASTNODE_IF_BRANCH
                ? "`if` branch"
                : (parent_kind == ASTNODE_WHILE
                   ? "`while` body"
                   : assert(0), "")),
            check->span);
        msg_addl_thin(&msg, "enclose body with `{}`");
        msg_emit(p, &msg);
    }
}

static AstNode* parse_if_branch(ParseCtx* p, Token* keyword, IfBranchKind kind) {
    AstNode* cond = NULL;

    if (kind != IFBR_ELSE) {
        expect_lparen(p);
        cond = parse_expr(p);
        expect_rparen(p);
    }

    AstNode* body = parse_expr(p);
    parse_check_body_for_elsetail_expr(p, body, ASTNODE_IF_BRANCH);
    // TODO: make `()` work too for enclosure
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
        AstNode* left = parse_identifier(p, p->prev, true);

        while (match(p, TOKEN_DOT)) {
            Token* dot = p->prev;
            if (match(p, TOKEN_STAR)) left = astnode_deref_new(left, dot, p->prev);
            else left = parse_access_expr(p, left, true);
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
            left = astnode_aggregate_literal_new(left, fields, p->prev);
        }
        return left;
    }/* else if (match(p, TOKEN_AT)) {
        return parse_directive(p);
    }*/ else if (match(p, TOKEN_LBRACE)) {
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
    } else if (match(p, TOKEN_KEYWORD_WHILE)) {
        Token* keyword = p->prev;
        expect_lparen(p);
        AstNode* cond = parse_expr(p);
        expect_rparen(p);

        bufpush(p->loop_breaks, NULL);
        bufpush(p->loop_continues, NULL);
        AstNode* mainbody = parse_expr(p);
        AstNode** breaks = p->loop_breaks[buflen(p->loop_breaks)-1];
        bufpop(p->loop_breaks);
        bufpop(p->loop_continues);
        parse_check_body_for_elsetail_expr(p, mainbody, ASTNODE_WHILE);

        AstNode* elsebody = NULL;
        if (match(p, TOKEN_KEYWORD_ELSE)) {
            elsebody = parse_expr(p);
            parse_check_body_for_elsetail_expr(p, elsebody, ASTNODE_WHILE);
        }

        return astnode_while_new(keyword, cond, mainbody, elsebody, breaks);
    } else if (match(p, TOKEN_KEYWORD_BREAK)) {
        Token* keyword = p->prev;
        AstNode* child = NULL;
        if (can_token_start_expr(p->current)) {
            child = parse_expr(p);
        }
        AstNode* astnode = astnode_break_new(keyword, child);
        if (buflen(p->loop_breaks) > 0) {
            bufpush(p->loop_breaks[buflen(p->loop_breaks)-1], astnode);
        } else {
            Msg msg = msg_with_span(
                MSG_ERROR,
                "`break` used outside a loop",
                keyword->span);
            msg_emit(p, &msg);
        }
        return astnode;
    } else if (match(p, TOKEN_KEYWORD_CONTINUE)) {
        Token* keyword = p->prev;
        if (buflen(p->loop_continues) == 0) {
            Msg msg = msg_with_span(
                MSG_ERROR,
                "`continue` used outside a loop",
                keyword->span);
            msg_emit(p, &msg);
        }
        return astnode_continue_new(keyword);
    } else if (match(p, TOKEN_KEYWORD_RETURN)) {
        Token* keyword = p->prev;
        AstNode* operand = NULL;
        if (can_token_start_expr(p->current)) {
            operand = parse_expr(p);
        }
        return astnode_return_new(keyword, operand);
    } else if (match(p, TOKEN_INTEGER_LITERAL)) {
        Token* token = p->prev;
        bigint val = bigint_new();
        // TODO: move so that these are only initialized once
        bigint base = bigint_new_u64(10);
        bigint digit = bigint_new();

        for (usize i = token->span.start; i < token->span.end; i++) {
            char c = token->span.srcfile->handle.contents[i];
            if (c != '_') {
                int d = c - '0';
                bigint_set_u64(&digit, (u64)d);
                bigint_mul(&val, &base);
                bigint_add(&val, &digit);
            }
        }

        bigint_free(&digit);
        bigint_free(&base);

        return astnode_integer_literal_new(token, val);
    } else if (match(p, TOKEN_STRING_LITERAL)) {
        return astnode_string_literal_new(p->prev);
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

        Token* rbrack = p->prev;
        AstNode* elem_type = NULL;
        if (can_token_start_typespec(p->current)) {
            elem_type = parse_typespec(p);
        }
        return astnode_array_literal_new(lbrack, elems, elem_type, elem_type ? elem_type->span : rbrack->span);
    } else if (match(p, TOKEN_LPAREN)) {
        AstNode* expr = parse_expr(p);
        expect_rparen(p);
        return expr;
    }
    // NOTE: Add the case to `can_token_begin_expr()`

    Msg msg = msg_with_span(
        MSG_ERROR,
        p->current->kind == TOKEN_SEMICOLON
        ? "unexpected `;`"
        : "expected expression",
        p->current->span);
    msg_emit(p, &msg);
    return NULL;
}

static AstNode* parse_suffix_expr(ParseCtx* p) {
    AstNode* left = parse_atom_expr(p);
    while (p->current->kind == TOKEN_LPAREN
           || p->current->kind == TOKEN_LBRACK
           || p->current->kind == TOKEN_DOT) {
        if (match(p, TOKEN_LPAREN)) {
            Token* lparen = p->prev;
            AstNode** args = parse_args(p, lparen, TOKEN_RPAREN, true, false);
            left = astnode_function_call_new(left, args, p->prev);
        } else if (match(p, TOKEN_LBRACK)) {
            AstNode* idx = parse_expr(p);
            Token* rbrack = expect_rbrack(p);
            left = astnode_index_new(left, idx, rbrack);
        } else if (match(p, TOKEN_DOT)) {
            Token* dot = p->prev;
            if (match(p, TOKEN_STAR)) {
                left = astnode_deref_new(left, dot, p->prev);
            } else {
                left = parse_access_expr(p, left, true);
            }
        }
        // NOTE: also add above in the while cond in `parse_suffix_expr` above
    }
    return left;
}

static AstNode* parse_unop_expr(ParseCtx* p) {
    if (match(p, TOKEN_MINUS) || match(p, TOKEN_BANG) || match(p, TOKEN_AMP)) {
        Token* op = p->prev;
        UnopKind kind;
        switch (op->kind) {
            case TOKEN_MINUS: kind = UNOP_NEG; break;
            case TOKEN_BANG:  kind = UNOP_BOOLNOT; break;
            case TOKEN_AMP:   kind = UNOP_ADDR; break;
        }
        AstNode* child = parse_unop_expr(p);
        return astnode_unop_new(kind, op, child);
    }
    return parse_suffix_expr(p);
}

static AstNode* parse_as_expr(ParseCtx* p) {
    AstNode* left = parse_unop_expr(p);
    if (match(p, TOKEN_KEYWORD_AS)) {
        Token* op = p->prev;
        AstNode* right = parse_typespec(p);
        left = astnode_cast_new(op, left, right);
    }
    return left;
}

static AstNode* parse_arithmul_binop_expr(ParseCtx* p) {
    AstNode* left = parse_as_expr(p);
    while (match(p, TOKEN_STAR) || match(p, TOKEN_FSLASH) || match(p, TOKEN_PERC)) {
        Token* op = p->prev;
        ArithBinopKind kind;
        switch (op->kind) {
            case TOKEN_STAR:   kind = ARITH_BINOP_MUL; break;
            case TOKEN_FSLASH: kind = ARITH_BINOP_DIV; break;
            case TOKEN_PERC:   kind = ARITH_BINOP_REM; break;
            default: assert(0); break;
        }
        AstNode* right = parse_as_expr(p);
        left = astnode_arith_binop_new(kind, op, left, right);
    }
    return left;
}

static AstNode* parse_arithadd_binop_expr(ParseCtx* p) {
    AstNode* left = parse_arithmul_binop_expr(p);
    while (match(p, TOKEN_PLUS) || match(p, TOKEN_MINUS)) {
        Token* op = p->prev;
        ArithBinopKind kind;
        switch (op->kind) {
            case TOKEN_PLUS:  kind = ARITH_BINOP_ADD; break;
            case TOKEN_MINUS: kind = ARITH_BINOP_SUB; break;
            default: assert(0); break;
        }
        AstNode* right = parse_arithmul_binop_expr(p);
        left = astnode_arith_binop_new(kind, op, left, right);
    }
    return left;
}

static AstNode* parse_booland_binop_expr(ParseCtx* p) {
    AstNode* left = parse_arithadd_binop_expr(p);
    while (match(p, TOKEN_KEYWORD_AND)) {
        Token* op = p->prev;
        AstNode* right = parse_arithadd_binop_expr(p);
        left = astnode_bool_binop_new(BOOL_BINOP_AND, op, left, right);
    }
    return left;
}

static AstNode* parse_boolor_binop_expr(ParseCtx* p) {
    AstNode* left = parse_booland_binop_expr(p);
    while (match(p, TOKEN_KEYWORD_OR)) {
        Token* op = p->prev;
        AstNode* right = parse_booland_binop_expr(p);
        left = astnode_bool_binop_new(BOOL_BINOP_OR, op, left, right);
    }
    return left;
}

static AstNode* parse_cmp_binop_expr(ParseCtx* p) {
    AstNode* left = parse_boolor_binop_expr(p);
    while (match(p, TOKEN_DOUBLE_EQUAL)
           || match(p, TOKEN_BANG_EQUAL)
           || match(p, TOKEN_LANGBR)
           || match(p, TOKEN_RANGBR)
           || match(p, TOKEN_LANGBR_EQUAL)
           || match(p, TOKEN_RANGBR_EQUAL)) {
        Token* op = p->prev;
        CmpBinopKind kind;
        switch (op->kind) {
            case TOKEN_DOUBLE_EQUAL: kind = CMP_BINOP_EQ; break;
            case TOKEN_BANG_EQUAL:   kind = CMP_BINOP_NE; break;
            case TOKEN_LANGBR:       kind = CMP_BINOP_LT; break;
            case TOKEN_RANGBR:       kind = CMP_BINOP_GT; break;
            case TOKEN_LANGBR_EQUAL: kind = CMP_BINOP_LE; break;
            case TOKEN_RANGBR_EQUAL: kind = CMP_BINOP_GE; break;
            default: assert(0); break;
        }
        AstNode* right = parse_boolor_binop_expr(p);
        left = astnode_cmp_binop_new(kind, op, left, right);
    }
    return left;
}

static AstNode* parse_assign_expr(ParseCtx* p) {
    AstNode* left = parse_cmp_binop_expr(p);
    if (match(p, TOKEN_EQUAL)) {
        Token* equal = p->prev;
        AstNode* right = parse_cmp_binop_expr(p);
        left = astnode_assign_new(equal, left, right);
    }
    return left;
}

static AstNode* parse_expr(ParseCtx* p) {
    return parse_assign_expr(p);
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

static AstNode* parse_variable_decl(ParseCtx* p, bool global) {
    Token* keyword = p->prev;
    bool immutable = true;
    if (keyword->kind == TOKEN_KEYWORD_MUT) immutable = false;
    Token* identifier = expect_identifier(p, "expected variable name");
    AstNode* typespec = NULL;
    if (match(p, TOKEN_COLON)) {
        typespec = parse_typespec(p);
    }
    Token* equal = NULL;
    AstNode* initializer = NULL;
    if (global) {
        equal = expect_equal(p);
        initializer = parse_expr(p);
    } else if (match(p, TOKEN_EQUAL)) {
        equal = p->prev;
        initializer = parse_expr(p);
    }
    expect_semicolon(p);
    return astnode_variable_decl_new(
        keyword,
        identifier,
        typespec,
        equal,
        initializer,
        !global,
        immutable);
}

static AstNode* parse_import(ParseCtx* p) {
    Token* keyword = p->prev;
    Token* arg = expect(p, TOKEN_STRING_LITERAL, "expected path string");
    Token* as = NULL;
    if (match(p, TOKEN_KEYWORD_AS)) {
        as = expect_identifier(p, "expected identifier to rename imported module");
    }
    expect_semicolon(p);

    if (is_token_lexeme(arg, "")) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "empty import",
            arg->span);
        msg_emit_non_fatal(p, &msg);
        return NULL;
    }

    // Denotes path wrt. file.
    char* path_wfile = token_tostring(arg);
    usize path_wfile_len = strlen(path_wfile);
    path_wfile += 1;
    path_wfile_len -= 1;
    path_wfile[--path_wfile_len] = '\0';

    const char* cur_path = p->srcfile->handle.path;
    // Denotes path wrt. compiler.
    char* path_wcomp = NULL;
    {
        isize last_fslash_idx = -1;
        for (const char* c = cur_path; *c != '\0'; c++) {
            if (*c == '/') last_fslash_idx = c - cur_path;
        }
        for (isize i = 0; i <= last_fslash_idx; i++) {
            bufpush(path_wcomp, cur_path[i]);
        }
        for (usize i = 0; i < path_wfile_len; i++) {
            bufpush(path_wcomp, path_wfile[i]);
        }
        bufpush(path_wcomp, '\0');
    }

    char* name = NULL;
    {
        usize last_dot_idx = path_wfile_len;
        usize last_fslash_idx = 0;
        for (usize i = 0; i < path_wfile_len; i++) {
            if (path_wfile[i] == '.') last_dot_idx = i;
            else if (path_wfile[i] == '/') {
                last_fslash_idx = i+1;
            }
        }
        for (usize i = last_fslash_idx; i < last_dot_idx; i++) {
            bufpush(name, path_wfile[i]);
        }
        bufpush(name, '\0');
    }

    Token* final_arg_token = as ? as : arg;
    char* final_name = as ? token_tostring(as) : name;

    /* printf(">> path_wfile: %s\n", path_wfile); */
    /* printf(">> path_wcomp: %s\n", path_wcomp); */
    /* printf(">> name: %s\n", name); */
    /* printf(">> final_name: %s\n", final_name); */

    Typespec* mod = read_srcfile(
        path_wcomp,
        span_some(arg->span),
        p->compile_ctx);

    if (mod) {
        return astnode_import_new(
            keyword,
            final_arg_token,
            mod,
            final_name);
    }
    p->error = true;
    return NULL;
}

static AstNode* parse_astnode_func_scope(ParseCtx* p, bool error_on_no_match) {
    if (match(p, TOKEN_KEYWORD_IMM) || match(p, TOKEN_KEYWORD_MUT)) {
        return parse_variable_decl(p, false);
    } else if (match(p, TOKEN_KEYWORD_IMPORT)) {
        return parse_import(p);
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

static AstNode* parse_scoped_block(ParseCtx* p) {
    Token* lbrace = p->prev;
    AstNode** stmts = NULL;
    Token* yield_keyword = NULL;
    AstNode* val = NULL;

    while (!match(p, TOKEN_RBRACE)) {
        check_eof(p, lbrace);
        AstNode* stmt = parse_astnode_func_scope(p, false);
        if (stmt) bufpush(stmts, stmt);
        else if (match(p, TOKEN_KEYWORD_YIELD)) {
            yield_keyword = p->prev;
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
                || (expr->kind == ASTNODE_WHILE
                    && (expr->whloop.elsebody
                        ? expr->whloop.elsebody->kind == ASTNODE_SCOPED_BLOCK
                        : expr->whloop.mainbody->kind == ASTNODE_SCOPED_BLOCK))
                || expr->kind == ASTNODE_SCOPED_BLOCK) {
            } else {
                expect_semicolon(p);
            }

            AstNode* exprstmt = astnode_exprstmt_new(expr);
            bufpush(stmts, exprstmt);
        }
    }
    return astnode_scoped_block_new(lbrace, stmts, yield_keyword, val, p->prev);
}

static AstNode* parse_astnode_root(ParseCtx* p) {
    if (p->current->kind == TOKEN_KEYWORD_EXPORT || p->current->kind == TOKEN_KEYWORD_FN) {
        Token* export = NULL;
        if (match(p, TOKEN_KEYWORD_EXPORT)) {
            export = p->prev;
        }
        expect(p, TOKEN_KEYWORD_FN, "expected `fn`");
        AstNode* header = parse_function_header(p);
        expect_lbrace(p);
        AstNode* body = parse_scoped_block(p);
        if (body->blk.val) {
            Msg msg = msg_with_span(
                MSG_ERROR,
                "`yield` cannot be used in a function block",
                body->blk.yield_keyword->span);
            msg_addl_thin(&msg, "`yield` can only be used in a scoped block");
            msg_addl_thin(&msg, "use `return` instead");
            msg_emit(p, &msg);
        }
        return astnode_function_def_new(export, header, body);
    } else if (match(p, TOKEN_KEYWORD_EXTERN)) {
        Token* extrn = p->prev;
        expect(p, TOKEN_KEYWORD_FN, "expected `fn`");
        AstNode* header = parse_function_header(p);
        expect_semicolon(p);
        return astnode_extern_function_new(extrn, header);
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
    } else if (match(p, TOKEN_KEYWORD_IMM) || match(p, TOKEN_KEYWORD_MUT)) {
        return parse_variable_decl(p, true);
    } else if (match(p, TOKEN_KEYWORD_IMPORT)) {
        return parse_import(p);
    }

    Msg msg = msg_with_span(
        MSG_ERROR,
        "expected a declaration or a definition",
        p->current->span);
    msg_emit(p, &msg);
    return NULL;
}

void parse(ParseCtx* p) {
    while (p->current->kind != TOKEN_EOF) {
        AstNode* astnode = parse_astnode_root(p);
        if (astnode) bufpush(p->srcfile->astnodes, astnode);
    }
}
