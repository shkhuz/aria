#include "sema.h"
#include "buf.h"
#include "msg.h"
#include "compile.h"

static inline ScopeInfo* sema_curscope(SemaCtx* s) {
    return buflast(s->scopebuf);
}

static inline void sema_scope_push(SemaCtx* s) {
    bufpush(s->scopebuf, (ScopeInfo){.decls = NULL});
}

SemaCtx sema_new_context(
    Srcfile* srcfile,
    CompileCtx* compile_ctx) {
    SemaCtx s;
    s.srcfile = srcfile;
    s.compile_ctx = compile_ctx;
    s.scopebuf = NULL;
    assert(sema_curscope(&s) == NULL);
    sema_scope_push(&s);
    assert(sema_curscope(&s) != NULL);
    s.error = false;
    return s;
}

static Typespec* sema_astnode(SemaCtx* s, AstNode* astnode);

static inline void msg_emit(SemaCtx* s, Msg* msg) {
    _msg_emit(msg, s->compile_ctx);
    if (msg->kind == MSG_ERROR) {
        s->error = true;
    }
}

static void sema_scope_declare(SemaCtx* s, char* identifier, AstNode* value, Span span) {
    bool found = false;
    bufrevloop(s->scopebuf, si) {
        bufloop(s->scopebuf[si].decls, i) {
            char* defined = s->scopebuf[si].decls[i].key;
            if (strcmp(defined, identifier) == 0) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "symbol is redeclared",
                    span);
                msg_addl_fat(&msg, "previous declaration here", s->scopebuf[si].decls[i].span);
                msg_emit(s, &msg);
                found = true;
                break;
            }
        }
        if (found) break;
    }

    if (!found) bufpush(sema_curscope(s)->decls, (TokenAstNodeTup){
        .key = identifier,
        .span = span,
        .value = value,
    });
}

static Typespec* sema_scope_retrieve(SemaCtx* s, Token* identifier) {
    if (is_token_lexeme(identifier, "u8")) return predef_typespecs.u8_type;
    else if (is_token_lexeme(identifier, "u16")) return predef_typespecs.u16_type;
    else if (is_token_lexeme(identifier, "u32")) return predef_typespecs.u32_type;
    else if (is_token_lexeme(identifier, "u64")) return predef_typespecs.u64_type;
    else if (is_token_lexeme(identifier, "i8")) return predef_typespecs.i8_type;
    else if (is_token_lexeme(identifier, "i16")) return predef_typespecs.i16_type;
    else if (is_token_lexeme(identifier, "i32")) return predef_typespecs.i32_type;
    else if (is_token_lexeme(identifier, "i64")) return predef_typespecs.i64_type;
    else if (is_token_lexeme(identifier, "void")) return predef_typespecs.void_type;

    bufrevloop(s->scopebuf, si) {
        bufloop(s->scopebuf[si].decls, i) {
            char* defined = s->scopebuf[si].decls[i].key;
            if (is_token_lexeme(identifier, defined)) {
                return s->scopebuf[si].decls[i].value->typespec;
            }
        }
    }
    Msg msg = msg_with_span(
        MSG_ERROR,
        "undefined symbol",
        identifier->span);
    msg_emit(s, &msg);
    return NULL;
}

static bool sema_assert_is_type(SemaCtx* s, Typespec* ty, AstNode* error_astnode) {
    assert(ty != NULL);
    if (ty->kind == TS_TYPE) {
        return true;
    }

    Msg msg = msg_with_span(
        MSG_ERROR,
        "expected type, got value",
        error_astnode->span);
    msg_emit(s, &msg);
    return false;
}

typedef enum {
    TC_OK,
    TC_ERROR,
    TC_ERROR_HANDLED,
} TypeComparisonResult;

static char* format_string_with_one_type(char* fmt, Typespec* ty, ...) {
    usize len = strlen(fmt);
    char* newfmt = NULL;
    for (usize i = 0; i < len; i++) {
        if (fmt[i] == '%' && i < len-1 && fmt[i+1] == 'T') {
            bufstrexpandpush(newfmt, typespec_tostring(ty));
            i++;
        } else if (fmt[i] == '%' && i == len-1) {
            assert(0 && "Unexpected EOS after '%'");
        } else {
            bufpush(newfmt, fmt[i]);
        }
    }
    bufpush(newfmt, '\0');

    char* newfmt2;
    va_list args;
    va_start(args, newfmt);
    vasprintf(&newfmt2, newfmt, args);
    va_end(args);
    buffree(newfmt);
    return newfmt2;
}

static char* format_string_with_two_types(char* fmt, Typespec* ty1, Typespec* ty2, ...) {
    usize len = strlen(fmt);
    char* newfmt = NULL;
    usize cur = 0;
    for (usize i = 0; i < len; i++) {
        if (fmt[i] == '%' && i < len-1 && fmt[i+1] == 'T') {
            bufstrexpandpush(newfmt, typespec_tostring(cur == 0 ? ty1 : ty2));
            i++;
            cur++;
        } else if (fmt[i] == '%' && i == len-1) {
            assert(0 && "Unexpected EOS after '%'");
        } else {
            bufpush(newfmt, fmt[i]);
        }
    }
    bufpush(newfmt, '\0');

    char* newfmt2;
    va_list args;
    va_start(args, newfmt);
    vasprintf(&newfmt2, newfmt, args);
    va_end(args);
    buffree(newfmt);
    return newfmt2;
}

static TypeComparisonResult sema_are_types_equal(SemaCtx* s, Typespec* from, Typespec* to, Span error) {
    if (from->kind == to->kind) {
        switch (from->kind) {
            case TS_PRIM: {
                if (from->prim.kind == to->prim.kind) return TC_OK;
                else if (from->prim.kind == PRIM_void || to->prim.kind == PRIM_void) return TC_ERROR;

                if (from->prim.kind == PRIM_comptime_integer) {
                    if (typespec_is_integer(to)) {
                        if (bigint_fits(
                            &from->prim.comptime_integer,
                            typespec_get_bytes(to),
                            typespec_is_signed(to))) {
                            return TC_OK;
                        } else {
                            Msg msg = msg_with_span(
                                MSG_ERROR,
                                format_string_with_one_type("integer value out of range for type `%T`", to),
                                error);
                            msg_addl_thin(
                                &msg,
                                format_string_with_one_type(
                                    "range of type `%T` is [%s, %s], which cannot fit integer `%s`",
                                    to,
                                    typespec_integer_get_min_value(to),
                                    typespec_integer_get_max_value(to),
                                    bigint_tostring(&from->prim.comptime_integer)));
                            msg_emit(s, &msg);
                            return TC_ERROR_HANDLED;
                        }
                    } else return TC_ERROR;
                } else if (to->prim.kind == PRIM_comptime_integer) {
                    assert(0);
                } else if (typespec_is_signed(from) == typespec_is_signed(to)) {
                    if (typespec_get_bytes(from) <= typespec_get_bytes(to)) return TC_OK;
                    else return TC_ERROR;
                } else return TC_ERROR;
            } break;

            case TS_PTR: {
                if (!from->ptr.immutable || to->ptr.immutable) {
                    return sema_are_types_equal(s, from->ptr.child, to->ptr.child, error);
                }
            } break;

            case TS_STRUCT: {
                if (from->agg == to->agg) return TC_OK;
                else return TC_ERROR;
            } break;

            case TS_TYPE: {
                return sema_are_types_equal(s, from->ty, to->ty, error);
            } break;
        }
    } else {
        return TC_ERROR;
    }

    assert(0);
    return TC_ERROR;
}

static bool sema_check_types_equal(SemaCtx* s, Typespec* from, Typespec* to, Span error) {
    TypeComparisonResult cmpresult = sema_are_types_equal(s, from, to, error);
    if (cmpresult == TC_ERROR) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            format_string_with_two_types("expected type `%T`, got `%T`", to, from),
            error);
        msg_emit(s, &msg);
    }
    return cmpresult == TC_OK ? true : false;
}

static void sema_top_level_decls_prec1(SemaCtx* s, AstNode* astnode) {
    switch (astnode->kind) {
        case ASTNODE_STRUCT: {
            Typespec* ty = typespec_type_new(typespec_struct_new(astnode));
            astnode->typespec = ty;
            sema_scope_declare(s, astnode->strct.name, astnode, astnode->strct.identifier->span);
        } break;
    }
}

static void sema_top_level_decls_prec2(SemaCtx* s, AstNode* astnode) {
    switch (astnode->kind) {
        case ASTNODE_VARIABLE_DECL: {
            Typespec* ty = NULL;
            if (astnode->vard.typespec) ty = sema_astnode(s, astnode->vard.typespec);
            if (ty) {
                if (sema_assert_is_type(s, ty, astnode->vard.typespec))
                    astnode->typespec = ty->ty;
            }
            sema_scope_declare(s, astnode->vard.name, astnode, astnode->vard.identifier->span);
        } break;

        case ASTNODE_FUNCTION_DEF: {
            bool error = false;
            Typespec** params_ty = NULL;
            AstNodeFunctionHeader* header = &astnode->funcdef.header->funch;
            bufloop(header->params, i) {
                Typespec* param_ty = sema_astnode(s, header->params[i]->paramd.typespec);
                if (param_ty) {
                    if (!sema_assert_is_type(s, param_ty, header->params[i]->paramd.typespec)) error = true;
                    bufpush(params_ty, param_ty->ty);
                } else error = true;
            }

            Typespec* ret_ty = sema_astnode(s, header->ret_typespec);
            if (ret_ty) {
                if (!sema_assert_is_type(s, ret_ty, header->ret_typespec)) error = true;
            } else error = true;

            if (!error) astnode->typespec = typespec_func_new(params_ty, ret_ty->ty);
            sema_scope_declare(s, header->name, astnode, header->identifier->span);
        } break;
    }
}

static Typespec* sema_typespec(SemaCtx* s, AstNode* astnode) {
    switch (astnode->kind) {
        case ASTNODE_TYPESPEC_PTR: {
            Typespec* child = sema_astnode(s, astnode->typeptr.child);
            if (child) {
                if (sema_assert_is_type(s, child, astnode->typeptr.child)) {
                    astnode->typespec = typespec_ptr_new(astnode->typeptr.immutable, child->ty);
                    return astnode->typespec;
                } else return NULL;
            } else return NULL;
        } break;
    }
    return NULL;
}

static bool sema_check_bigint_overflow(SemaCtx* s, bigint* b, Span span) {
    if (b->neg && bigint_fits(b, 8, true)) {
        return false;
    } else if (!b->neg && bigint_fits(b, 8, false)) {
        return false;
    } else {
        Msg msg = msg_with_span(
            MSG_ERROR,
            b->neg ? "integer underflow" : "integer overflow",
            span);
        msg_addl_thin(
            &msg,
            format_string(
                "integer value `%s` exceeds largest cpu register (64-bits)",
                bigint_tostring(b)));
        msg_emit(s, &msg);
        return true;
    }
}

static Typespec* sema_astnode(SemaCtx* s, AstNode* astnode) {
    switch (astnode->kind) {
        case ASTNODE_INTEGER_LITERAL: {
            if (sema_check_bigint_overflow(s, &astnode->intl.val, astnode->span)) {
                return NULL;
            } else {
                Typespec* ty = typespec_comptime_integer_new(astnode->intl.val);
                astnode->typespec = ty;
                return ty;
            }
        } break;

        case ASTNODE_UNOP: {
            switch (astnode->unop.kind) {
                case UNOP_NEG: {
                    Typespec* ty = sema_astnode(s, astnode->unop.child);
                    if (ty) {
                        if (typespec_is_integer(ty)) {
                            if (typespec_is_signed(ty)) {
                                astnode->typespec = ty;
                                return ty;
                            } else {
                                Msg msg = msg_with_span(
                                    MSG_ERROR,
                                    format_string_with_one_type("expected signed integer, got `%T`", ty),
                                    astnode->unop.op->span);
                                msg_emit(s, &msg);
                                return NULL;
                            }
                        } else if (typespec_is_comptime_integer(ty)) {
                            bigint new = bigint_new();
                            bigint_copy(&new, &ty->prim.comptime_integer);
                            bigint_neg(&new);
                            if (sema_check_bigint_overflow(s, &new, astnode->span)) return NULL;

                            Typespec* tynew = typespec_comptime_integer_new(new);
                            astnode->typespec = tynew;
                            return tynew;
                        } else {
                            Msg msg = msg_with_span(
                                MSG_ERROR,
                                format_string_with_one_type("expected integer operand, got type `%T`", ty),
                                astnode->unop.op->span);
                            msg_emit(s, &msg);
                            return NULL;
                        }
                    } else return NULL;
                } break;
            }
        } break;

        case ASTNODE_BINOP: {
            Typespec* left_ty = sema_astnode(s, astnode->binop.left);
            Typespec* right_ty = sema_astnode(s, astnode->binop.right);
            if (left_ty && right_ty) {
                bool left_is_integer = typespec_is_integer(left_ty);
                bool right_is_integer = typespec_is_integer(right_ty);
                bool left_is_comptime_integer = typespec_is_comptime_integer(left_ty);
                bool right_is_comptime_integer = typespec_is_comptime_integer(right_ty);
                bool left_is_anyint = left_is_integer || left_is_comptime_integer;
                bool right_is_anyint = right_is_integer || right_is_comptime_integer;

                #define sema_astnode_binop_nonint_operand(name, ty) \
                    Msg msg = msg_with_span( \
                        MSG_ERROR, \
                        format_string_with_one_type("expected integer " name " operand, got `%T`", ty), \
                        astnode->binop.op->span); \
                    msg_emit(s, &msg); \

                if (left_is_anyint && right_is_anyint) {
                    if (left_is_integer && right_is_integer) {
                        if (typespec_is_signed(left_ty) == typespec_is_signed(right_ty)) {
                            if (typespec_get_bytes(left_ty) >= typespec_get_bytes(right_ty)) {
                                astnode->typespec = left_ty;
                            } else {
                                astnode->typespec = right_ty;
                            }
                            return astnode->typespec;
                        } else {
                            Msg msg = msg_with_span(
                                MSG_ERROR,
                                format_string_with_two_types("mismatched operand signedness: `%T` and `%T`", left_ty, right_ty),
                                astnode->binop.op->span);
                            msg_emit(s, &msg);
                            return NULL;
                        }
                    } else if (left_is_comptime_integer && right_is_comptime_integer) {
                        bigint new = bigint_new();
                        bigint_copy(&new, &left_ty->prim.comptime_integer);
                        bigint_add(&new, &right_ty->prim.comptime_integer);
                        if (sema_check_bigint_overflow(s, &new, astnode->span)) return NULL;

                        Typespec* tynew = typespec_comptime_integer_new(new);
                        astnode->typespec = tynew;
                        return tynew;
                    } else {
                        // TODO
                    }
                } else {
                    if (!left_is_anyint) {
                        sema_astnode_binop_nonint_operand("left", left_ty);
                    }
                    if (!right_is_anyint) {
                        sema_astnode_binop_nonint_operand("right", right_ty);
                    }
                    return NULL;
                }
            } else return NULL;
        } break;

        case ASTNODE_STRUCT: {
            bool error = false;
            bufloop(astnode->strct.fields, i) {
                AstNode* fieldnode = astnode->strct.fields[i];
                Typespec* field_ty = sema_astnode(s, fieldnode->field.value);
                if (field_ty) {
                    if (sema_assert_is_type(s, field_ty, fieldnode->field.value))
                        fieldnode->typespec = field_ty->ty;
                    else error = true;
                } else error = true;
            }
            return error ? NULL : astnode->typespec;
        } break;

        case ASTNODE_TYPESPEC_PTR: {
            Typespec* ty = sema_typespec(s, astnode);
            return ty == NULL ? NULL : typespec_type_new(ty);
        } break;

        case ASTNODE_FUNCTION_DEF: {
            if (!astnode->funcdef.global) {
                sema_top_level_decls_prec2(s, astnode);
            }
            sema_astnode(s, astnode->funcdef.body);
        } break;

        case ASTNODE_SCOPED_BLOCK: {
            bool error = false;
            bufloop(astnode->blk.stmts, i) {
                if (!sema_astnode(s, astnode->blk.stmts[i])) error = true;
            }
            return error ? NULL : NULL;
        } break;

        case ASTNODE_FUNCTION_CALL: {
            Typespec* callee_ty = sema_astnode(s, astnode->funcc.callee);
            if (callee_ty) {
                if (callee_ty->kind == TS_FUNC) {
                    usize params_len = buflen(callee_ty->func.params);
                    usize args_len = buflen(astnode->funcc.args);
                    if (args_len == params_len) {
                        bool error = false;
                        Typespec** args_ty = NULL;
                        bufloop(astnode->funcc.args, i) {
                            Typespec* arg_ty = sema_astnode(s, astnode->funcc.args[i]);
                            if (arg_ty) {
                                if (!sema_check_types_equal(s, arg_ty, callee_ty->func.params[i], astnode->funcc.args[i]->span)) error = true;
                            } else error = true;
                        }

                    } else if (args_len < params_len) {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            format_string("expected %d argument(s), found %d", params_len, args_len),
                            astnode->funcc.rparen->span);
                        msg_addl_thin(&msg, format_string_with_one_type("expected argument of type `%T`", callee_ty->func.params[args_len]));
                        msg_emit(s, &msg);
                        return NULL;
                    } else if (args_len > params_len) {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            format_string("expected %d argument(s), found %d", params_len, args_len),
                            span_from_two(astnode->funcc.args[params_len]->span, astnode->funcc.args[args_len-1]->span));
                        msg_emit(s, &msg);
                        return NULL;
                    }
                } else {
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        format_string_with_one_type("attempting to call type `%T`", callee_ty),
                        astnode->funcc.callee->span);
                    msg_emit(s, &msg);
                    return NULL;
                }
            } else return NULL;
        } break;

        case ASTNODE_EXPRSTMT: {
            sema_astnode(s, astnode->exprstmt);
        } break;

        case ASTNODE_SYMBOL: {
            Typespec* ty = sema_scope_retrieve(s, astnode->sym.identifier);
            astnode->typespec = ty;
            return ty;
        } break;

        case ASTNODE_ACCESS: {
            Typespec* left = sema_astnode(s, astnode->acc.left);
            if (left) {
                assert(astnode->acc.right->kind == ASTNODE_SYMBOL);
                Token* right = astnode->acc.right->sym.identifier;
                if (left->kind == TS_STRUCT) {
                    AstNode** fields = left->agg->strct.fields;
                    bufloop(fields, i) {
                        if (are_token_lexemes_equal(
                                fields[i]->field.key,
                                right)) {
                            return fields[i]->field.value->typespec;
                        }
                    }
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        "`.`: symbol not found",
                        astnode->acc.right->span);
                    msg_emit(s, &msg);
                    return NULL;
                } else if (left->kind == TS_TYPE) {
                    // TODO: search for members in type
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        "`.`: symbol not found in type",
                        astnode->acc.right->span);
                    msg_emit(s, &msg);
                    return NULL;
                } else if (left->kind == TS_MODULE) {
                    AstNode** astnodes = left->mod.srcfile->astnodes;
                    bufloop(astnodes, i) {
                        if (is_token_lexeme(right, astnode_get_name(astnodes[i]))) {
                            return astnodes[i]->typespec;
                        }
                    }
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        "`.`: symbol not found",
                        astnode->acc.right->span);
                    msg_emit(s, &msg);
                    return NULL;
                }
            } else return NULL;
        } break;

        case ASTNODE_GENERIC_TYPESPEC: {
            Typespec* left = sema_astnode(s, astnode->genty.left);
        } break;

        case ASTNODE_IMPORT: {
            sema_scope_declare(s, astnode->import.name, astnode, astnode->import.arg->span);
        } break;

        case ASTNODE_VARIABLE_DECL: {
            if (astnode->vard.stack) {
                sema_top_level_decls_prec2(s, astnode);
            } else {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "[internal] global variables not implemented",
                    astnode->span);
                msg_emit(s, &msg);
                return NULL;
            }

            Typespec* annotated = astnode->typespec;
            Typespec* initializer = sema_astnode(s, astnode->vard.initializer);
            if (annotated && initializer) {
                if (sema_check_types_equal(s, initializer, annotated, astnode->vard.equal->span))
                    return annotated;
                else return NULL;
            } else if (initializer) {
                astnode->typespec = initializer;
                return initializer;
            } else return NULL;
        } break;
    }

    return NULL;
}

bool sema(SemaCtx* sema_ctxs) {
    bool error = false;
    bufloop(sema_ctxs, i) {
        SemaCtx* s = &sema_ctxs[i];
        bufloop(s->srcfile->astnodes, j) {
            sema_top_level_decls_prec1(s, s->srcfile->astnodes[j]);
        }
        if (s->error) error = true;
    }
    if (error) return true;

    bufloop(sema_ctxs, i) {
        SemaCtx* s = &sema_ctxs[i];
        bufloop(s->srcfile->astnodes, j) {
            sema_top_level_decls_prec2(s, s->srcfile->astnodes[j]);
        }
        if (s->error) error = true;
    }
    if (error) return true;

    bufloop(sema_ctxs, i) {
        SemaCtx* s = &sema_ctxs[i];
        bufloop(s->srcfile->astnodes, j) {
            sema_astnode(s, s->srcfile->astnodes[j]);
        }
        if (s->error) error = true;
    }
    if (error) return true;

    return false;
}
