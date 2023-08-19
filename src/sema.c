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

static inline void sema_scope_pop(SemaCtx* s) {
    bufpop(s->scopebuf);
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
    s.current_func = NULL;
    return s;
}

static Typespec* sema_astnode(SemaCtx* s, AstNode* astnode, Typespec* target);

static inline void msg_emit(SemaCtx* s, Msg* msg) {
    _msg_emit(msg, s->compile_ctx);
    if (msg->kind == MSG_ERROR) {
        s->error = true;
    }
}

static bool sema_scope_declare(SemaCtx* s, char* identifier, AstNode* value, Span span) {
    bool found = false;
    bufrevloop(s->scopebuf, si) {
        bufloop(s->scopebuf[si].decls, i) {
            char* defined = s->scopebuf[si].decls[i].key;
            if (strcmp(defined, identifier) == 0) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    (si == buflen(s->scopebuf)-1) ? "symbol is redeclared" : "symbol shadows another symbol",
                    span);
                msg_addl_fat(&msg, "previous declaration here:", s->scopebuf[si].decls[i].span);
                msg_emit(s, &msg);
                found = true;
                break;
            }
        }
        if (found) break;
    }

    if (found) return false;
    else {
        bufpush(sema_curscope(s)->decls, (TokenAstNodeTup){
            .key = identifier,
            .span = span,
            .value = value,
        });
        return true;
    }
}

typedef struct {
    Typespec* type;
    AstNode* ref;
} ScopeRetrieveInfo;

static AstNode* sema_scope_retrieve(SemaCtx* s, Token* identifier) {
    bufrevloop(s->scopebuf, si) {
        bufloop(s->scopebuf[si].decls, i) {
            char* defined = s->scopebuf[si].decls[i].key;
            if (is_token_lexeme(identifier, defined)) {
                return s->scopebuf[si].decls[i].value;
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

typedef enum {
    TC_OK,
    TC_MISMATCH,
    TC_CONST,
    TC_ERROR_HANDLED,
} TypeComparisonResult;

typedef struct {
    TypeComparisonResult result;
    Typespec* final;
} TypeComparisonInfo;

static bool sema_are_types_exactly_equal(SemaCtx* s, Typespec* from, Typespec* to) {
    if (from->kind == to->kind) {
        switch (from->kind) {
            case TS_PRIM: {
                if (from->prim.kind == to->prim.kind) return true;
                else return false;
            } break;

            case TS_PTR: {
                if (from->ptr.immutable == to->ptr.immutable) {
                    return sema_are_types_exactly_equal(s, from->ptr.child, to->ptr.child);
                } else return false;
            } break;

            case TS_MULTIPTR: {
                if (from->mulptr.immutable == to->mulptr.immutable) {
                    return sema_are_types_exactly_equal(s, from->mulptr.child, to->mulptr.child);
                } else return false;
            } break;

            case TS_SLICE: {
                if (from->slice.immutable == to->slice.immutable) {
                    return sema_are_types_exactly_equal(s, from->slice.child, to->slice.child);
                } else return false;
            } break;

            case TS_ARRAY: {
                if (bigint_cmp(&from->array.size->prim.integer, &to->array.size->prim.integer) == 0) {
                    return sema_are_types_exactly_equal(s, from->array.child, to->array.child);
                } else return false;
            } break;

            case TS_STRUCT: {
                if (from->agg == to->agg) return true;
                return false;
            } break;

            case TS_TYPE: {
                assert(0 && "given TS_TYPE to sema_are_types_exactly_equal()");
            } break;

            case TS_FUNC: {
                if (buflen(from->func.params) == buflen(to->func.params)) {
                    bool same = true;
                    bufloop(from->func.params, i) {
                        if (!sema_are_types_exactly_equal(
                                s,
                                from->func.params[i],
                                to->func.params[i])) {
                            same = false;
                            break;
                        }
                    }

                    if (!same) return false;
                    return sema_are_types_exactly_equal(
                        s,
                        from->func.ret_typespec,
                        to->func.ret_typespec);
                } else return false;
            } break;

            case TS_NORETURN: {
                return true;
            } break;

            default: assert(0);
        }
    }
    return false;
}

static TypeComparisonInfo sema_are_types_equal(SemaCtx* s, Typespec* from, Typespec* to, Span error) {
    if (from->kind == to->kind) {
        switch (from->kind) {
            case TS_PRIM: {
                if (from->prim.kind == to->prim.kind) return (TypeComparisonInfo){ .result = TC_OK, .final = to };
                else if (typespec_is_integer(from) && typespec_is_integer(to)) {
                    if (from->prim.kind == PRIM_integer || to->prim.kind == PRIM_integer) {
                        Typespec* sized = from->prim.kind == PRIM_integer ? to : from;
                        Typespec* unsized = from->prim.kind == PRIM_integer ? from : to;

                        if (bigint_fits(
                            &unsized->prim.integer,
                            typespec_get_bytes(sized),
                            typespec_is_signed(sized))) {
                            return (TypeComparisonInfo){ .result = TC_OK, .final = sized };
                        } else {
                            Msg msg = msg_with_span(
                                MSG_ERROR,
                                format_string_with_one_type(
                                    "type `%T` cannot fit integer `%s`",
                                    sized,
                                    bigint_tostring(&unsized->prim.integer)),
                                error);
                            msg_emit(s, &msg);
                            return (TypeComparisonInfo){ .result = TC_ERROR_HANDLED, .final = NULL };
                        }
                    } else if (typespec_is_signed(from) == typespec_is_signed(to)) {
                        if (typespec_get_bytes(from) <= typespec_get_bytes(to)) return (TypeComparisonInfo){ .result = TC_OK, .final = to };
                        else return (TypeComparisonInfo){ .result = TC_MISMATCH, .final = NULL };
                    } else return (TypeComparisonInfo){ .result = TC_MISMATCH, .final = NULL };
                } else return (TypeComparisonInfo){ .result = TC_MISMATCH, .final = NULL };
            } break;

            case TS_PTR: {
                if (!from->ptr.immutable || to->ptr.immutable) {
                    if (sema_are_types_exactly_equal(s, from->ptr.child, to->ptr.child)) {
                        return (TypeComparisonInfo){ .result = TC_OK, .final = to };
                    } else {
                        return (TypeComparisonInfo){ .result = TC_MISMATCH, .final = NULL };
                    }
                } else return (TypeComparisonInfo){ .result = TC_CONST, .final = NULL };
            } break;

            case TS_MULTIPTR: {
                if (!from->mulptr.immutable || to->mulptr.immutable) {
                    if (sema_are_types_exactly_equal(s, from->mulptr.child, to->mulptr.child)) {
                        return (TypeComparisonInfo){ .result = TC_OK, .final = to };
                    } else {
                        return (TypeComparisonInfo){ .result = TC_MISMATCH, .final = NULL };
                    }
                } else return (TypeComparisonInfo){ .result = TC_CONST, .final = NULL };
            } break;

            case TS_SLICE: {
                if (!from->slice.immutable || to->slice.immutable) {
                    if (sema_are_types_exactly_equal(s, from->slice.child, to->slice.child)) {
                        return (TypeComparisonInfo){ .result = TC_OK, .final = to };
                    } else {
                        return (TypeComparisonInfo){ .result = TC_MISMATCH, .final = NULL };
                    }
                } else return (TypeComparisonInfo){ .result = TC_CONST, .final = NULL };
            } break;

            case TS_ARRAY: {
                if (bigint_cmp(&from->array.size->prim.integer, &to->array.size->prim.integer) == 0) {
                    if (sema_are_types_exactly_equal(s, from->array.child, to->array.child)) {
                        return (TypeComparisonInfo){ .result = TC_OK, .final = to };
                    } else {
                        return (TypeComparisonInfo){ .result = TC_MISMATCH, .final = NULL };
                    }
                } else return (TypeComparisonInfo){ .result = TC_MISMATCH, .final = NULL };
            } break;

            case TS_STRUCT: {
                if (from->agg == to->agg) return (TypeComparisonInfo){ .result = TC_OK, .final = to };
                else return (TypeComparisonInfo){ .result = TC_MISMATCH, .final = NULL };
            } break;

            case TS_TYPE: {
                assert(0 && "given TS_TYPE to sema_are_types_equal()");
            } break;

            case TS_FUNC: {
                if (buflen(from->func.params) == buflen(to->func.params)) {
                    bool same = true;
                    bufloop(from->func.params, i) {
                        TypeComparisonInfo cmpinfo = sema_are_types_equal(
                                s,
                                from->func.params[i],
                                to->func.params[i],
                                error);
                        if (cmpinfo.result != TC_OK) {
                            same = false;
                            break;
                        }
                    }

                    if (!same) return (TypeComparisonInfo){ .result = TC_MISMATCH, .final = NULL };
                    return sema_are_types_equal(
                        s,
                        from->func.ret_typespec,
                        to->func.ret_typespec,
                        error);
                } else return (TypeComparisonInfo){ .result = TC_MISMATCH, .final = NULL };
            } break;

            case TS_NORETURN: {
                return (TypeComparisonInfo){ .result = TC_OK, .final = to };
            } break;
        }
    } else if (typespec_is_arrptr(from) && (to->kind == TS_MULTIPTR || to->kind == TS_SLICE)) {
        bool to_imm;
        Typespec* to_child = NULL;
        switch (to->kind) {
            case TS_MULTIPTR: to_imm = to->mulptr.immutable; to_child = to->mulptr.child; break;
            case TS_SLICE:    to_imm = to->slice.immutable;  to_child = to->slice.child; break;
            default: assert(0);
        }

        if (!from->ptr.immutable || to_imm) {
            if (sema_are_types_exactly_equal(s, from->ptr.child->array.child, to_child)) {
                return (TypeComparisonInfo){ .result = TC_OK, .final = to };
            } else {
                return (TypeComparisonInfo){ .result = TC_MISMATCH, .final = NULL };
            }
        } else return (TypeComparisonInfo){ .result = TC_CONST, .final = NULL };
    } else if (from->kind == TS_NORETURN || to->kind == TS_NORETURN) {
        if (from->kind == TS_NORETURN) return (TypeComparisonInfo){ .result = TC_OK, .final = to };
        else return (TypeComparisonInfo){ .result = TC_OK, .final = from };
    } else {
        return (TypeComparisonInfo){ .result = TC_MISMATCH, .final = NULL };
    }

    assert(0);
    return (TypeComparisonInfo){ .result = TC_MISMATCH, .final = NULL };
}

static Typespec* sema_check_types_equal(SemaCtx* s, Typespec* from, Typespec* to, bool peer, Span error) {
    TypeComparisonInfo cmpinfo = sema_are_types_equal(s, from, to, error);
    if (cmpinfo.result != TC_OK && cmpinfo.result != TC_ERROR_HANDLED) {
        if (peer) {
            Msg msg = msg_with_span(
                MSG_ERROR,
                format_string_with_two_types("type mismatch: `%T` and `%T`", from, to),
                error);
            if (cmpinfo.result == TC_CONST) {
                msg_addl_thin(&msg, "type mismatch due to change in immutability");
            }
            msg_emit(s, &msg);
        } else {
            Msg msg = msg_with_span(
                MSG_ERROR,
                format_string_with_two_types("expected type `%T`, got `%T`", to, from),
                error);
            if (cmpinfo.result == TC_CONST) {
                msg_addl_thin(&msg, "type mismatch due to change in immutability");
            }
            msg_emit(s, &msg);
        }
    }
    return cmpinfo.result == TC_OK ? cmpinfo.final : NULL;
}

typedef enum {
    AT_RUNTIME = 1 << 0,

    // Compile-time types which are not emitted.
    AT_COMPTIME = 1 << 1,
    AT_VOID = 1 << 2,
    AT_FUNC = 1 << 3,
    AT_NORETURN = 1 << 4,

    AT_DEFAULT_VALUE = AT_RUNTIME | AT_COMPTIME | AT_VOID,
    AT_STORAGE_TYPE = AT_RUNTIME | AT_COMPTIME,
    AT_RETURN_TYPE = AT_RUNTIME | AT_VOID | AT_NORETURN,
} AcceptTypespecKind;

typedef struct {
    bool accept_runtime;
    bool accept_comptime;
    bool accept_void;
    bool accept_func;
    bool accept_noreturn;
} AcceptTypespecDecoded;

static AcceptTypespecKind get_accept_params(Typespec* ty) {
    AcceptTypespecKind given_ty;
    if (typespec_is_void(ty))         return AT_VOID;
    else if (ty->kind == TS_FUNC)     return AT_FUNC;
    else if (ty->kind == TS_NORETURN) return AT_NORETURN;
    else if (typespec_is_sized(ty))   return AT_RUNTIME;
    else                              return AT_COMPTIME;
}

static AcceptTypespecDecoded get_decoded_accept_params(AcceptTypespecKind accept) {
    AcceptTypespecDecoded a;
    a.accept_runtime = (accept&AT_RUNTIME) == AT_RUNTIME;
    a.accept_comptime = (accept&AT_COMPTIME) == AT_COMPTIME;
    a.accept_void = (accept&AT_VOID) == AT_VOID;
    a.accept_func = (accept&AT_FUNC) == AT_FUNC;
    a.accept_noreturn = (accept&AT_NORETURN) == AT_NORETURN;
    return a;
}

static bool sema_check_isvalue(SemaCtx* s, Typespec* ty, AcceptTypespecKind accept) {
    if (ty->kind == TS_TYPE || ty->kind == TS_MODULE) {
        return false;
    }

    AcceptTypespecKind given_ty = get_accept_params(ty);
    AcceptTypespecDecoded a = get_decoded_accept_params(accept);

    if ((a.accept_runtime && given_ty == AT_RUNTIME)
        || (a.accept_comptime && given_ty == AT_COMPTIME)
        || (a.accept_void && given_ty == AT_VOID)
        || (a.accept_func && given_ty == AT_FUNC)
        || (a.accept_noreturn && given_ty == AT_NORETURN)) {
        return true;
    } else return false;
}

static bool sema_verify_isvalue(SemaCtx* s, Typespec* ty, AcceptTypespecKind accept, Span error) {
    if (ty->kind == TS_TYPE || ty->kind == TS_MODULE) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            format_string("expected value, got %s", ty->kind == TS_TYPE ? "type" : "module"),
            error);
        msg_emit(s, &msg);
        return false;
    }

    AcceptTypespecKind given_ty = get_accept_params(ty);
    AcceptTypespecDecoded a = get_decoded_accept_params(accept);

    if ((a.accept_runtime && given_ty == AT_RUNTIME)
        || (a.accept_comptime && given_ty == AT_COMPTIME)
        || (a.accept_void && given_ty == AT_VOID)
        || (a.accept_func && given_ty == AT_FUNC)
        || (a.accept_noreturn && given_ty == AT_NORETURN)) {
        return true;
    } else if (a.accept_runtime && given_ty == AT_COMPTIME) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            format_string_with_one_type("expected runtime value, got `%T`", ty),
            error);
        msg_emit(s, &msg);
        return false;
    } else if (given_ty == AT_NORETURN && !a.accept_noreturn) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "value is of type `noreturn`: parent code unreachable",
            error);
        msg_emit(s, &msg);
        return false;
    } else {
        Msg msg = msg_with_span(
            MSG_ERROR,
            format_string_with_one_type("expected value, got `%T`", ty),
            error);
        msg_emit(s, &msg);
        return false;
    }
}

static bool sema_verify_istype(SemaCtx* s, Typespec* ty, AcceptTypespecKind accept, Span error) {
    if (ty->kind != TS_TYPE) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            format_string("expected type, got %s", ty->kind == TS_MODULE ? "module" : "value"),
            error);
        msg_emit(s, &msg);
        return false;
    }

    AcceptTypespecKind given_ty = get_accept_params(ty->ty);
    AcceptTypespecDecoded a = get_decoded_accept_params(accept);

    if ((a.accept_runtime && given_ty == AT_RUNTIME)
        || (a.accept_comptime && given_ty == AT_COMPTIME)
        || (a.accept_void && given_ty == AT_VOID)
        || (a.accept_func && given_ty == AT_FUNC)
        || (a.accept_noreturn && given_ty == AT_NORETURN)) {
        return true;
    } else {
        Msg msg = msg_with_span(
            MSG_ERROR,
            format_string_with_one_type("type `%T` is invalid here", ty->ty),
            error);
        msg_emit(s, &msg);
        return false;
    }
}

static bool sema_verify_ismod(SemaCtx* s, Typespec* ty, Span error) {
    if (ty->kind == TS_MODULE) {
        return true;
    } else {
        Msg msg = msg_with_span(
            MSG_ERROR,
            format_string("expected module, got %s", ty->kind == TS_TYPE ? "type" : "value"),
            error);
        msg_emit(s, &msg);
        return false;
    }
}

static bool sema_check_is_lvalue(SemaCtx* s, AstNode* astnode) {
    if (astnode->kind == ASTNODE_ACCESS
        || astnode->kind == ASTNODE_DEREF
        || astnode->kind == ASTNODE_INDEX
        || astnode->kind == ASTNODE_SYMBOL
        || astnode->kind == ASTNODE_BUILTIN_SYMBOL) {
        if ((astnode->kind == ASTNODE_SYMBOL || astnode->kind == ASTNODE_BUILTIN_SYMBOL)
            && (astnode->typespec->kind == TS_TYPE
                || astnode->typespec->kind == TS_MODULE)) {
            assert(0 && "Symbol is not lvalue: not a valuetype");
        }
        return true;
    } else {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "expression not an l-value",
            astnode->span);
        msg_emit(s, &msg);
        return false;
    }
}

static bool sema_is_lvalue_imm(AstNode* astnode) {
    if ((astnode->kind == ASTNODE_ACCESS && (astnode->acc.left->typespec->kind == TS_PTR))
        || astnode->kind == ASTNODE_DEREF
        || (astnode->kind == ASTNODE_INDEX && (astnode->idx.left->typespec->kind == TS_MULTIPTR
                                               || astnode->idx.left->typespec->kind == TS_SLICE))) {
        AstNode* child;
        switch (astnode->kind) {
            case ASTNODE_ACCESS: child = astnode->acc.left; break;
            case ASTNODE_DEREF: child = astnode->deref.child; break;
            case ASTNODE_INDEX: child = astnode->idx.left; break;
            default: assert(0);
        }

        switch (child->typespec->kind) {
            case TS_PTR: return child->typespec->ptr.immutable;
            case TS_MULTIPTR: return child->typespec->mulptr.immutable;
            case TS_SLICE: return child->typespec->slice.immutable;
            default: assert(0);
        }
    }

    switch (astnode->kind) {
        case ASTNODE_SYMBOL: {
            AstNode* ref = astnode->sym.ref;
            switch (ref->kind) {
                case ASTNODE_FUNCTION_DEF: return true;
                case ASTNODE_VARIABLE_DECL: return ref->vard.immutable;
                case ASTNODE_PARAM_DECL: return false;
                default: assert(0);
            }
        } break;

        case ASTNODE_ACCESS: {
            return sema_is_lvalue_imm(astnode->acc.left);
        } break;

        case ASTNODE_DEREF: {
            return sema_is_lvalue_imm(astnode->deref.child);
        } break;

        case ASTNODE_INDEX: {
            return sema_is_lvalue_imm(astnode->idx.left);
        } break;

        default: assert(0);
    }
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

static bool sema_check_variable_type(SemaCtx* s, AstNode* astnode) {
    if (astnode->vard.typespec) {
        Typespec* ty = sema_astnode(s, astnode->vard.typespec, NULL);
        if (ty && sema_verify_istype(s, ty, AT_STORAGE_TYPE, astnode->vard.typespec->span)) {
            astnode->typespec = ty->ty;
            return true;
        } return false;
    } else return true;
}

static bool sema_declare_variable(SemaCtx* s, AstNode* astnode) {
    return sema_scope_declare(s, astnode->vard.name, astnode, astnode->vard.identifier->span);
}

static void sema_top_level_decls_prec2(SemaCtx* s, AstNode* astnode) {
    switch (astnode->kind) {
        case ASTNODE_VARIABLE_DECL: {
            sema_check_variable_type(s, astnode);
            sema_declare_variable(s, astnode);
        } break;

        case ASTNODE_FUNCTION_DEF: {
            bool error = false;
            Typespec** params_ty = NULL;
            AstNodeFunctionHeader* header = &astnode->funcdef.header->funch;
            bufloop(header->params, i) {
                AstNode* param = header->params[i];
                Typespec* ty = sema_astnode(s, param->paramd.typespec, NULL);
                if (ty && sema_verify_istype(s, ty, AT_STORAGE_TYPE, param->paramd.typespec->span)) {
                    param->typespec = ty->ty;
                    bufpush(params_ty, ty->ty);
                } else error = true;
            }

            Typespec* ret_ty = sema_astnode(s, header->ret_typespec, NULL);
            if (ret_ty && sema_verify_istype(s, ret_ty, AT_RETURN_TYPE, header->ret_typespec->span)) {
            } else error = true;

            if (!error) {
                astnode->funcdef.header->typespec = typespec_func_new(params_ty, ret_ty->ty);
                astnode->typespec = astnode->funcdef.header->typespec;
            }
            sema_scope_declare(s, header->name, astnode, header->identifier->span);
        } break;

        case ASTNODE_STRUCT: {
            bool error = false;
            bufloop(astnode->strct.fields, i) {
                AstNode* fieldnode = astnode->strct.fields[i];
                Typespec* field_ty = sema_astnode(s, fieldnode->field.value, NULL);
                if (field_ty && sema_verify_istype(s, field_ty, AT_STORAGE_TYPE, fieldnode->field.value->span)) {
                    fieldnode->typespec = field_ty->ty;
                } else error = true;
            }
        } break;
    }
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
                "integer `%s` exceeds largest cpu register (64-bits)",
                bigint_tostring(b)));
        msg_emit(s, &msg);
        return true;
    }
}

static AstNode* sema_access_field_from_type(SemaCtx* s, Typespec* ty, Token* key, Span error, bool derefed) {
    if (ty->kind == TS_STRUCT) {
        AstNode** fields = ty->agg->strct.fields;
        bufloop(fields, i) {
            if (are_token_lexemes_equal(
                    fields[i]->field.key,
                    key)) {
                return fields[i];
            }
        }
        Msg msg = msg_with_span(
            MSG_ERROR,
            format_string_with_one_type("symbol not found in type `%T`%s", ty, derefed ? " (dereferenced)" : ""),
            error);
        msg_emit(s, &msg);
        return NULL;
    } else if (ty->kind == TS_TYPE) {
        assert(0 && "TODO: implement");
    } else if (ty->kind == TS_MODULE) {
        AstNode** astnodes = ty->mod.srcfile->astnodes;
        bufloop(astnodes, i) {
            if (is_token_lexeme(key, astnode_get_name(astnodes[i]))) {
                return astnodes[i];
            }
        }
        Msg msg = msg_with_span(
            MSG_ERROR,
            "symbol not found",
            error);
        msg_emit(s, &msg);
        return NULL;
    } else if (ty->kind == TS_PRIM) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            format_string_with_one_type("symbol not found in type `%T`%s", ty, derefed ? " (dereferenced)" : ""),
            error);
        msg_emit(s, &msg);
        return NULL;
    } else if (ty->kind == TS_PTR) {
        return sema_access_field_from_type(s, ty->ptr.child, key, error, true);
    } else {
        Msg msg = msg_with_span(
            MSG_ERROR,
            format_string_with_one_type("field access is not supported by type `%T`%s", ty, derefed ? " (dereferenced)" : ""),
            error);
        msg_emit(s, &msg);
        return NULL;
    }
}

static Typespec* sema_index_type(SemaCtx* s, Typespec* ty, Span error, bool derefed) {
    if (ty->kind == TS_MULTIPTR) {
        return ty->mulptr.child;
    } else if (ty->kind == TS_SLICE) {
        return ty->slice.child;
    } else if (ty->kind == TS_ARRAY) {
        if (bigint_cmp(&ty->array.size->prim.integer, &BIGINT_ZERO) != 0) {
            return ty->array.child;
        } else {
            Msg msg = msg_with_span(
                MSG_ERROR,
                format_string_with_one_type("indexing zero-length array with type `%T`%s", ty, derefed ? " (dereferenced)" : ""),
                error);
            msg_emit(s, &msg);
            return NULL;
        }
    } else if (typespec_is_arrptr(ty)) {
        return sema_index_type(s, ty->ptr.child, error, true);
    } else {
        Msg msg = msg_with_span(
            MSG_ERROR,
            format_string_with_one_type("cannot index type `%T`%s", ty, derefed ? " (dereferenced)" : ""),
            error);
        if (ty->kind == TS_PTR) {
            msg_addl_thin(
                &msg,
                format_string_with_one_type(
                    "consider using a multi-ptr: `[*]%s%T`",
                    ty->ptr.child,
                    ty->ptr.immutable ? "imm " : ""));
        }
        msg_emit(s, &msg);
        return NULL;
    }
}

static bool sema_if_branch(
    SemaCtx* s,
    AstNode* iff,
    AstNode* br,
    bool* first_br_val_valid,
    Typespec** first_br_val_type,
    Span* first_br_val_span,
    bool* else_req_if_value_br_error)
{
    bool error = false;
    if (br->ifbr.kind != IFBR_ELSE) {
        Typespec* cond = sema_astnode(s, br->ifbr.cond, NULL);
        if (cond && sema_verify_isvalue(s, cond, AT_DEFAULT_VALUE, br->ifbr.cond->span)) {
            if (sema_check_types_equal(s, cond, predef_typespecs.bool_type->ty, false, br->ifbr.cond->span)) {
                br->ifbr.cond->typespec = predef_typespecs.bool_type->ty;
            } else error = true;
        } else error = true;
    }

    Span cur_br_val_span =
        br->ifbr.body->kind != ASTNODE_SCOPED_BLOCK
        ? br->ifbr.body->span
        : (br->ifbr.body->blk.val
           ? br->ifbr.body->blk.val->span
           : br->ifbr.body->blk.rbrace->span);
    Typespec* body = sema_astnode(s, br->ifbr.body, NULL);
    if (body && sema_verify_isvalue(s, body, AT_DEFAULT_VALUE|AT_NORETURN, br->ifbr.body->span)) {
        if (*first_br_val_valid) {
            TypeComparisonInfo cmpinfo = sema_are_types_equal(
                s,
                body,
                *first_br_val_type,
                cur_br_val_span);
            if (cmpinfo.result == TC_OK) {
                br->typespec = cmpinfo.final;
                iff->typespec = cmpinfo.final;
            } else if (cmpinfo.result == TC_ERROR_HANDLED) {
                error = true;
            } else {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    format_string_with_one_type("`if` branch type mismatch: this branch yields `%T`...", body),
                    cur_br_val_span);
                msg_addl_fat(
                    &msg,
                    format_string_with_one_type("...while first branch yields `%T`:", *first_br_val_type),
                    *first_br_val_span);
                msg_emit(s, &msg);
                error = true;
            }
        } else if (br->ifbr.kind == IFBR_IF) {
            *first_br_val_valid = true;
            *first_br_val_type = body;
            *first_br_val_span = cur_br_val_span;
            iff->typespec = *first_br_val_type;
        }
    } else error = true;

    if (body
        && !typespec_is_void(body)
        && body->kind != TS_NORETURN
        && !iff->iff.elsebr
        && !(*else_req_if_value_br_error)) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            "`else` clause required if any `if` branch yields a value",
            iff->span);
        msg_addl_fat(&msg, "value yielded here:", cur_br_val_span);
        msg_emit(s, &msg);
        *else_req_if_value_br_error = true;
        error = true;
    }
    return error;
}

static Typespec* sema_scoped_block(SemaCtx* s, AstNode* astnode, Typespec* target, bool open_new_scope) {
    if (open_new_scope) sema_scope_push(s);
    bool error = false;
    AstNode* noreturn_stmt = NULL;
    usize noreturn_stmt_idx;
    bufloop(astnode->blk.stmts, i) {
        Typespec* stmt = sema_astnode(s, astnode->blk.stmts[i], NULL);
        if (stmt) {
            if (astnode->blk.stmts[i]->kind == ASTNODE_EXPRSTMT && stmt->kind == TS_NORETURN && !noreturn_stmt) {
                noreturn_stmt = astnode->blk.stmts[i];
                noreturn_stmt_idx = i;
            }
        } else error = true;
    }
    if (astnode->blk.val) {
        Typespec* val = sema_astnode(s, astnode->blk.val, target);
        if (val && sema_verify_isvalue(s, val, AT_DEFAULT_VALUE, astnode->blk.val->span)) {
            astnode->typespec = val;
        } else error = true;
    } else {
        astnode->typespec = predef_typespecs.void_type->ty;
    }

    if (noreturn_stmt) {
        astnode->typespec = predef_typespecs.noreturn_type->ty;
        if (noreturn_stmt_idx == buflen(astnode->blk.stmts)-1) {
            if (astnode->blk.val) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "unreachable code",
                    astnode->blk.val->span);
                msg_addl_fat(&msg, "due to this:", noreturn_stmt->span);
                msg_emit(s, &msg);
                error = true;
            }
        } else {
            Msg msg = msg_with_span(
                MSG_ERROR,
                "unreachable code",
                astnode->blk.stmts[noreturn_stmt_idx+1]->span);
            msg_addl_fat(&msg, "due to this:", noreturn_stmt->span);
            msg_emit(s, &msg);
            error = true;
        }
    }
    if (open_new_scope) sema_scope_pop(s);
    return error ? NULL : astnode->typespec;
}

static Typespec* sema_astnode(SemaCtx* s, AstNode* astnode, Typespec* target) {
    switch (astnode->kind) {
        case ASTNODE_INTEGER_LITERAL: {
            if (sema_check_bigint_overflow(s, &astnode->intl.val, astnode->span)) {
                return NULL;
            } else {
                Typespec* ty = typespec_unsized_integer_new(astnode->intl.val);
                astnode->typespec = ty;
                return ty;
            }
        } break;

        case ASTNODE_STRING_LITERAL: {
            Token* lit = astnode->strl.token;
            bigint lit_size = bigint_new_u64((lit->span.end-1) - (lit->span.start+1));
            astnode->typespec = typespec_ptr_new(
                true,
                typespec_array_new(
                    typespec_unsized_integer_new(lit_size),
                    predef_typespecs.u8_type->ty));
            return astnode->typespec;
        } break;

        case ASTNODE_ARRAY_LITERAL: {
            Typespec* elem_annotated = NULL;
            if (astnode->arrayl.elem_type) {
                Typespec* typeof_annotated = sema_astnode(s, astnode->arrayl.elem_type, NULL);
                if (typeof_annotated && sema_verify_istype(s, typeof_annotated, AT_STORAGE_TYPE, astnode->arrayl.elem_type->span)) {
                    elem_annotated = typeof_annotated->ty;
                } else return NULL;
            } else if (target && target->kind == TS_ARRAY) {
                elem_annotated = target->array.child;
            }

            if (buflen(astnode->arrayl.elems) == 0 && !elem_annotated) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "cannot infer type of empty array literal",
                    astnode->span);
                msg_emit(s, &msg);
                return NULL;
            }

            if (buflen(astnode->arrayl.elems) != 0) {
                bool error = false;
                AstNode** elems = astnode->arrayl.elems;
                bool unsized_elems = false;
                Typespec* final_elem_type = NULL;

                if (elem_annotated)
                    final_elem_type = elem_annotated;
                else {
                    final_elem_type = sema_astnode(s, elems[0], NULL);
                    if (final_elem_type && sema_verify_isvalue(s, final_elem_type, AT_DEFAULT_VALUE, elems[0]->span)) {
                        if (!typespec_is_sized(final_elem_type)) unsized_elems = true;
                    } else error = true;
                }

                for (usize i = elem_annotated ? 0 : 1; i < buflen(elems); i++) {
                    Typespec* cur_elem_type = sema_astnode(s, elems[i], final_elem_type);
                    if (cur_elem_type && sema_verify_isvalue(s, cur_elem_type, AT_DEFAULT_VALUE, elems[i]->span)) {
                        if (!typespec_is_sized(cur_elem_type)) unsized_elems = true;
                        if (sema_check_types_equal(s, cur_elem_type, final_elem_type, false, elems[i]->span)) {
                            elems[i]->typespec = final_elem_type;
                        } else error = true;
                    } else error = true;
                }

                if (unsized_elems && (final_elem_type == NULL || (final_elem_type && !typespec_is_sized(final_elem_type)))) {
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        elem_annotated ? "elements and annotated type are unsized" : "elements are unsized and no type annotation provided",
                        astnode->span);
                    msg_emit(s, &msg);
                    error = true;
                }

                if (error) return NULL;
                bigint size = bigint_new_u64(buflen(elems));
                astnode->typespec = typespec_array_new(typespec_unsized_integer_new(size), final_elem_type);
                return astnode->typespec;
            } else {
                astnode->typespec = typespec_array_new(typespec_unsized_integer_new(BIGINT_ZERO), elem_annotated);
                return astnode->typespec;
            }
        } break;

        case ASTNODE_UNOP: {
            Typespec* child = sema_astnode(s, astnode->unop.child, NULL);
            switch (astnode->unop.kind) {
                case UNOP_NEG: {
                    if (child && sema_verify_isvalue(s, child, AT_DEFAULT_VALUE, astnode->unop.child->span)) {
                        if (typespec_is_sized_integer(child)) {
                            if (typespec_is_signed(child)) {
                                astnode->typespec = child;
                                return child;
                            } else {
                                Msg msg = msg_with_span(
                                    MSG_ERROR,
                                    format_string_with_one_type("expected signed integer, got `%T`", child),
                                    astnode->short_span);
                                msg_emit(s, &msg);
                                return NULL;
                            }
                        } else if (typespec_is_unsized_integer(child)) {
                            bigint new = bigint_new();
                            bigint_copy(&new, &child->prim.integer);
                            bigint_neg(&new);
                            if (sema_check_bigint_overflow(s, &new, astnode->span)) return NULL;

                            Typespec* tynew = typespec_unsized_integer_new(new);
                            astnode->typespec = tynew;
                            return tynew;
                        } else {
                            Msg msg = msg_with_span(
                                MSG_ERROR,
                                format_string_with_one_type("expected integer operand, got type `%T`", child),
                                astnode->short_span);
                            msg_emit(s, &msg);
                            return NULL;
                        }
                    } else return NULL;
                } break;

                case UNOP_BOOLNOT: {
                    if (child && sema_verify_isvalue(s, child, AT_DEFAULT_VALUE, astnode->unop.child->span)) {
                        if (sema_check_types_equal(s, child, predef_typespecs.bool_type->ty, false, astnode->unop.child->span)) {
                            astnode->typespec = predef_typespecs.bool_type->ty;
                            return astnode->typespec;
                        } else return NULL;
                    } else return NULL;
                } break;

                case UNOP_ADDR: {
                    if (child && sema_verify_isvalue(s, child, AT_RUNTIME|AT_FUNC/*TODO: check error handling*/, astnode->unop.child->span)) {
                        if (sema_check_is_lvalue(s, astnode->unop.child)) {
                            bool imm = sema_is_lvalue_imm(astnode->unop.child);
                            astnode->typespec = typespec_ptr_new(imm, child);
                            return astnode->typespec;
                        } else return NULL;
                    } else return NULL;
                } break;
            }
        } break;

        case ASTNODE_DEREF: {
            Typespec* child = sema_astnode(s, astnode->deref.child, NULL);
            if (child && sema_verify_isvalue(s, child, AT_DEFAULT_VALUE, astnode->deref.child->span)) {
                if (child->kind == TS_PTR && sema_check_isvalue(s, child->ptr.child, AT_RUNTIME)) {
                    astnode->typespec = child->ptr.child;
                    return astnode->typespec;
                } else {
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        format_string_with_one_type("cannot dereference type `%T`", child),
                        astnode->short_span);
                    if (child->kind == TS_MULTIPTR) {
                        msg_addl_thin(
                            &msg,
                            format_string_with_one_type(
                                "consider using a single-ptr: `*%s%T`",
                                child->mulptr.child,
                                child->mulptr.immutable ? "imm " : ""));
                    }
                    msg_emit(s, &msg);
                    return NULL;
                }
            } else return NULL;
        } break;

        case ASTNODE_INDEX: {
            Typespec* left = sema_astnode(s, astnode->idx.left, NULL);
            Typespec* idx = sema_astnode(s, astnode->idx.idx, NULL);
            bool error = false;
            if (left && sema_verify_isvalue(s, left, AT_DEFAULT_VALUE, astnode->idx.left->span)) {
                Typespec* after_idx = sema_index_type(s, left, astnode->idx.left->span, false);
                if (after_idx) astnode->typespec = after_idx;
                else error = true;
            } else error = true;

            if (idx && sema_verify_isvalue(s, idx, AT_DEFAULT_VALUE, astnode->idx.idx->span)) {
                if (typespec_is_sized_integer(idx)) {
                    if (typespec_is_signed(idx)) {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            format_string_with_one_type("index requires an unsigned integer, got `%T`", idx),
                            astnode->idx.idx->span);
                        msg_emit(s, &msg);
                        error = true;
                    }
                } else if (typespec_is_unsized_integer(idx)) {
                    if (idx->prim.integer.neg) {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            format_string("index requires a non-negative integer, got `%s`", bigint_tostring(&idx->prim.integer)),
                            astnode->idx.idx->span);
                        msg_emit(s, &msg);
                        error = true;
                    }
                } else {
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        format_string_with_one_type("index requires an unsigned integer, got `%T`", idx),
                        astnode->idx.idx->span);
                    msg_emit(s, &msg);
                    error = true;
                }
            } else error = true;

            return error ? NULL : astnode->typespec;
        } break;

        case ASTNODE_ARITH_BINOP: {
            Typespec* left = sema_astnode(s, astnode->arthbin.left, NULL);
            Typespec* right = sema_astnode(s, astnode->arthbin.right, NULL);
            if (left && right) {
                bool left_valid = sema_verify_isvalue(s, left, AT_DEFAULT_VALUE, astnode->arthbin.left->span);
                bool right_valid = sema_verify_isvalue(s, right, AT_DEFAULT_VALUE, astnode->arthbin.right->span);
                if (left_valid && right_valid) {
                    bool left_is_sized_integer = typespec_is_sized_integer(left);
                    bool right_is_sized_integer = typespec_is_sized_integer(right);
                    bool left_is_unsized_integer = typespec_is_unsized_integer(left);
                    bool right_is_unsized_integer = typespec_is_unsized_integer(right);
                    bool left_is_anyint = left_is_sized_integer || left_is_unsized_integer;
                    bool right_is_anyint = right_is_sized_integer || right_is_unsized_integer;

                    #define sema_astnode_arith_binop_nonint_operand(name, ty) \
                        Msg msg = msg_with_span( \
                            MSG_ERROR, \
                            format_string_with_one_type("expected integer " name " operand, got `%T`", ty), \
                            astnode->short_span); \
                        msg_emit(s, &msg); \

                    if (left_is_anyint && right_is_anyint) {
                        if (left_is_sized_integer && right_is_sized_integer) {
                            if (typespec_is_signed(left) == typespec_is_signed(right)) {
                                if (typespec_get_bytes(left) >= typespec_get_bytes(right)) {
                                    astnode->typespec = left;
                                } else {
                                    astnode->typespec = right;
                                }
                                return astnode->typespec;
                            } else {
                                Msg msg = msg_with_span(
                                    MSG_ERROR,
                                    format_string_with_two_types("mismatched operand signedness: `%T` and `%T`", left, right),
                                    astnode->short_span);
                                msg_emit(s, &msg);
                                return NULL;
                            }
                        } else if (left_is_unsized_integer && right_is_unsized_integer) {
                            bigint new = bigint_new();
                            if (astnode->arthbin.kind == ARITH_BINOP_ADD
                                || astnode->arthbin.kind == ARITH_BINOP_SUB
                                || astnode->arthbin.kind == ARITH_BINOP_MUL) {
                                bigint_copy(&new, &left->prim.integer);
                                switch (astnode->arthbin.kind) {
                                    case ARITH_BINOP_ADD:
                                        bigint_add(&new, &right->prim.integer); break;
                                    case ARITH_BINOP_SUB:
                                        bigint_sub(&new, &right->prim.integer); break;
                                    case ARITH_BINOP_MUL:
                                        bigint_mul(&new, &right->prim.integer); break;
                                    default: assert(0);
                                }
                            } else if (astnode->arthbin.kind == ARITH_BINOP_DIV
                                       || astnode->arthbin.kind == ARITH_BINOP_REM) {
                                if (bigint_cmp(&right->prim.integer, &BIGINT_ZERO) == 0) {
                                    Msg msg = msg_with_span(
                                        MSG_ERROR,
                                        "cannot divide by zero",
                                        astnode->short_span);
                                    msg_emit(s, &msg);
                                    return NULL;
                                } else {
                                    bigint second = bigint_new();
                                    switch (astnode->arthbin.kind) {
                                        case ARITH_BINOP_DIV: {
                                            bigint_div_mod(
                                                &left->prim.integer,
                                                &right->prim.integer,
                                                &new,
                                                &second);
                                        } break;

                                        case ARITH_BINOP_REM: {
                                            bigint_div_mod(
                                                &left->prim.integer,
                                                &right->prim.integer,
                                                &second,
                                                &new);
                                        } break;
                                        default: assert(0);
                                    }
                                }
                            } else assert(0);

                            if (sema_check_bigint_overflow(s, &new, astnode->span)) return NULL;

                            Typespec* tynew = typespec_unsized_integer_new(new);
                            astnode->typespec = tynew;
                            return tynew;
                        } else {
                            Typespec* unsized_integer = left->prim.kind == PRIM_integer ? left : right;
                            Typespec* sized_integer = left->prim.kind == PRIM_integer ? right : left;
                            bool is_left_unsized_integer = left->prim.kind == PRIM_integer;

                            if (bigint_fits(
                                &unsized_integer->prim.integer,
                                typespec_get_bytes(sized_integer),
                                typespec_is_signed(sized_integer))) {
                                astnode->typespec = sized_integer;
                                return astnode->typespec;
                            } else {
                                Msg msg = msg_with_span(
                                    MSG_ERROR,
                                    format_string_with_one_type(
                                        "%s operand `%s` cannot fit in %s operand's type `%T`",
                                        sized_integer,
                                        is_left_unsized_integer ? "left" : "right",
                                        bigint_tostring(&unsized_integer->prim.integer),
                                        is_left_unsized_integer ? "right" : "left"),
                                    astnode->short_span);
                                // TODO: add range help note
                                msg_emit(s, &msg);
                                return NULL;
                            }
                        }
                    } else {
                        if (!left_is_anyint) {
                            sema_astnode_arith_binop_nonint_operand("left", left);
                        }
                        if (!right_is_anyint) {
                            sema_astnode_arith_binop_nonint_operand("right", right);
                        }
                        return NULL;
                    }
                }
            } else return NULL;
        } break;

        case ASTNODE_BOOL_BINOP: {
            Typespec* left = sema_astnode(s, astnode->boolbin.left, NULL);
            Typespec* right = sema_astnode(s, astnode->boolbin.right, NULL);
            if (left && right) {
                bool left_valid = sema_verify_isvalue(s, left, AT_DEFAULT_VALUE, astnode->boolbin.left->span);
                bool right_valid = sema_verify_isvalue(s, right, AT_DEFAULT_VALUE, astnode->boolbin.right->span);
                if (left_valid && right_valid) {
                    bool left_bool = sema_check_types_equal(s, left, predef_typespecs.bool_type->ty, false, astnode->boolbin.left->span);
                    bool right_bool = sema_check_types_equal(s, right, predef_typespecs.bool_type->ty, false, astnode->boolbin.right->span);
                    if (left_bool && right_bool) {
                        astnode->typespec = predef_typespecs.bool_type->ty;
                        return astnode->typespec;
                    } else return NULL;
                } else return NULL;
            } else return NULL;
        } break;

        case ASTNODE_CMP_BINOP: {
            Typespec* left = sema_astnode(s, astnode->cmpbin.left, NULL);
            Typespec* right = sema_astnode(s, astnode->cmpbin.right, NULL);
            if (left && right) {
                bool left_valid = sema_verify_isvalue(s, left, AT_DEFAULT_VALUE, astnode->cmpbin.left->span);
                bool right_valid = sema_verify_isvalue(s, right, AT_DEFAULT_VALUE, astnode->cmpbin.right->span);
                if (left_valid && right_valid) {
                    if (sema_check_types_equal(s, left, right, true, astnode->short_span)) {
                        astnode->typespec = predef_typespecs.bool_type->ty;
                        return astnode->typespec;
                    } else return NULL;
                } else return NULL;
            } else return NULL;

        } break;

        case ASTNODE_ASSIGN: {
            Typespec* left = sema_astnode(s, astnode->assign.left, NULL);
            Typespec* right = sema_astnode(s, astnode->assign.right, left);
            bool error = false;

            if (left && sema_verify_isvalue(s, left, AT_DEFAULT_VALUE, astnode->assign.left->span)) {
                if (sema_check_is_lvalue(s, astnode->assign.left)) {
                    bool imm = sema_is_lvalue_imm(astnode->assign.left);
                    if (imm) {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            "cannot assign to constant",
                            astnode->assign.left->span);
                        msg_emit(s, &msg);
                        error = true;
                    }
                } else error = true;

                if (right && sema_verify_isvalue(s, right, AT_DEFAULT_VALUE, astnode->assign.right->span)) {
                    if (sema_check_types_equal(s, right, left, false, astnode->short_span)) {
                        astnode->typespec = predef_typespecs.void_type->ty;
                    } else error = true;
                }
            } else error = true;

            return error ? NULL : astnode->typespec;
        } break;

        case ASTNODE_CAST: {
            Typespec* left = sema_astnode(s, astnode->cast.left, NULL);
            Typespec* right = sema_astnode(s, astnode->cast.right, NULL);
            bool left_valid = left && sema_verify_isvalue(s, left, AT_DEFAULT_VALUE, astnode->cast.left->span);
            bool right_valid = right && sema_verify_istype(
                s,
                right,
                // Maybe this is not neccesary as it accepts all types,
                // and checks it on its own.
                AT_RUNTIME|AT_VOID|AT_FUNC|AT_NORETURN,
                astnode->cast.right->span);

            if (left_valid && right_valid) {
                TypeComparisonResult result = TC_OK;
                Typespec* inright = right->ty;

                if (left->kind == TS_PRIM && inright->kind == TS_PRIM) {
                    if (left->prim.kind == PRIM_void && inright->prim.kind == PRIM_void) {}
                    else if (left->prim.kind == PRIM_void || inright->prim.kind == PRIM_void) result = TC_MISMATCH;
                    else {}
                } else if (typespec_is_arrptr(left) && (inright->kind == TS_MULTIPTR || inright->kind == TS_SLICE)) {
                    // TODO: maybe check for immutability
                } else if ((left->kind == TS_PTR || left->kind == TS_MULTIPTR || left->kind == TS_SLICE) &&
                           (inright->kind == TS_PTR || inright->kind == TS_MULTIPTR || inright->kind == TS_SLICE)) {
                    // TODO: maybe check for immutability
                    // TODO: take care of alignment
                } else if (left->kind == TS_ARRAY && inright->kind == TS_ARRAY) {
                    if (bigint_cmp(&left->array.size->prim.integer, &inright->array.size->prim.integer) == 0) {
                        if (sema_are_types_exactly_equal(s, left->array.child, inright->array.child)) {}
                        else result = TC_MISMATCH;
                    } else result = TC_MISMATCH;
                } else if (left->kind == TS_STRUCT && inright->kind == TS_STRUCT) {
                    if (left->agg == right->agg) {}
                    else result = TC_MISMATCH;
                } else if (left->kind == TS_TYPE && inright->kind == TS_TYPE) {
                    assert(0);
                } else result = TC_MISMATCH;

                if (result != TC_OK) {
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        format_string_with_two_types("cannot cast type `%T` to `%T`", left, inright),
                        astnode->short_span);
                    msg_emit(s, &msg);
                    return NULL;
                } else {
                    astnode->typespec = inright;
                    return astnode->typespec;
                }
            } else return NULL;
        } break;

        case ASTNODE_TYPESPEC_FUNC: {
            bool error = false;
            Typespec** params = NULL;
            bufloop(astnode->typefunc.params, i) {
                Typespec* param = sema_astnode(s, astnode->typefunc.params[i], NULL);
                if (param && sema_verify_istype(s, param, AT_STORAGE_TYPE, astnode->typefunc.params[i]->span)) {
                    bufpush(params, param->ty);
                } else error = true;
            }

            Typespec* ret = sema_astnode(s, astnode->typefunc.ret_typespec, NULL);
            if (ret && sema_verify_istype(s, ret, AT_RETURN_TYPE, astnode->typefunc.ret_typespec->span)) {
            } else error = true;

            if (error) return NULL;
            astnode->typespec = typespec_type_new(typespec_func_new(params, ret->ty));
            return astnode->typespec;
        } break;

        case ASTNODE_TYPESPEC_PTR: {
            Typespec* child = sema_astnode(s, astnode->typeptr.child, NULL);
            if (child && sema_verify_istype(s, child, AT_RUNTIME|AT_VOID|AT_FUNC, astnode->typeptr.child->span)) {
                astnode->typespec = typespec_type_new(typespec_ptr_new(astnode->typeptr.immutable, child->ty));
                return astnode->typespec;
            } else return NULL;
        } break;

        case ASTNODE_TYPESPEC_MULTIPTR: {
            Typespec* child = sema_astnode(s, astnode->typemulptr.child, NULL);
            if (child && sema_verify_istype(s, child, AT_RUNTIME|AT_VOID|AT_FUNC, astnode->typeptr.child->span)) {
                astnode->typespec = typespec_type_new(typespec_multiptr_new(astnode->typemulptr.immutable, child->ty));
                return astnode->typespec;
            } else return NULL;
        } break;

        case ASTNODE_TYPESPEC_SLICE: {
            Typespec* child = sema_astnode(s, astnode->typeslice.child, NULL);
            if (child && sema_verify_istype(s, child, AT_STORAGE_TYPE, astnode->typeslice.child->span)) {
                astnode->typespec = typespec_type_new(typespec_slice_new(astnode->typeslice.immutable, child->ty));
                return astnode->typespec;
            } else return NULL;
        } break;

        case ASTNODE_TYPESPEC_ARRAY: {
            Typespec* child = sema_astnode(s, astnode->typearray.child, NULL);
            Typespec* size = sema_astnode(s, astnode->typearray.size, NULL);
            bool error = false;

            if (child && sema_verify_istype(s, child, AT_STORAGE_TYPE, astnode->typearray.child->span)) {
            } else error = true;
            if (size && sema_verify_isvalue(s, size, AT_DEFAULT_VALUE, astnode->typearray.size->span)) {
                if (typespec_is_unsized_integer(size)) {
                    if (size->prim.integer.neg) {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            format_string(
                                "array size should be a non-negative integer, got `%s`",
                                bigint_tostring(&size->prim.integer)),
                            astnode->typearray.size->span);
                        msg_emit(s, &msg);
                        error = true;
                    }
                } else {
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        format_string_with_one_type("array size should be a compile-time known integer, got `%T`", size),
                        astnode->typearray.size->span);
                    msg_emit(s, &msg);
                    error = true;
                }
            } else error = true;

            if (error) return NULL;
            astnode->typespec = typespec_type_new(typespec_array_new(size, child->ty));
            return astnode->typespec;
        } break;

        case ASTNODE_FUNCTION_DEF: {
            s->current_func = astnode;
            sema_scope_push(s);
            bool error = false;
            AstNodeFunctionHeader* header = &astnode->funcdef.header->funch;
            bufloop(header->params, i) {
                AstNode* param = header->params[i];
                if (!sema_scope_declare(s, param->paramd.name, param, param->paramd.identifier->span)) error = true;
            }
            if (!sema_scoped_block(s, astnode->funcdef.body, astnode->funcdef.header->funch.ret_typespec->typespec->ty, false)) error = true;
            sema_scope_pop(s);
            s->current_func = NULL;

            return error ? NULL : astnode->typespec;
        } break;

        case ASTNODE_SCOPED_BLOCK: {
            return sema_scoped_block(s, astnode, target, true);
        } break;

        case ASTNODE_FUNCTION_CALL: {
            Typespec* callee_ty = sema_astnode(s, astnode->funcc.callee, NULL);
            if (callee_ty && sema_verify_isvalue(s, callee_ty, AT_DEFAULT_VALUE|AT_FUNC, astnode->funcc.callee->span)) {
                if (callee_ty->kind == TS_FUNC ||
                    (callee_ty->kind == TS_PTR && callee_ty->ptr.child->kind == TS_FUNC)) {

                    Typespec* func_ty = NULL;
                    if (callee_ty->kind == TS_FUNC) {
                        // callee can be TS_FUNC in only two cases:
                        // 1) it is a symbol, or
                        // 2) it is an access expression
                        if (astnode->funcc.callee->kind == ASTNODE_SYMBOL)
                            astnode->funcc.ref = astnode->funcc.callee->sym.ref;
                        else if (astnode->funcc.callee->kind == ASTNODE_ACCESS)
                            astnode->funcc.ref = astnode->funcc.callee->acc.accessed;
                        else assert(0 && "callee is not symbol/accessor expr when type is TS_FUNC");
                        func_ty = callee_ty;
                    } else if  (callee_ty->kind == TS_PTR && callee_ty->ptr.child->kind == TS_FUNC) {
                        func_ty = callee_ty->ptr.child;
                    }

                    usize params_len = buflen(func_ty->func.params);
                    usize args_len = buflen(astnode->funcc.args);
                    if (args_len == params_len) {
                        bool error = false;
                        bufloop(astnode->funcc.args, i) {
                            Typespec* arg_ty = sema_astnode(s, astnode->funcc.args[i], func_ty->func.params[i]);
                            if (arg_ty && sema_verify_isvalue(s, arg_ty, AT_DEFAULT_VALUE, astnode->funcc.args[i]->span)) {
                                if (!sema_check_types_equal(s, arg_ty, func_ty->func.params[i], false, astnode->funcc.args[i]->span)) error = true;
                            } else error = true;
                        }

                        if (!error) {
                            astnode->typespec = func_ty->func.ret_typespec;
                            return astnode->typespec;
                        } else return NULL;
                    } else if (args_len < params_len) {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            format_string("expected %d argument(s), found %d", params_len, args_len),
                            astnode->funcc.rparen->span);
                        msg_addl_thin(&msg, format_string_with_one_type("expected argument of type `%T`", func_ty->func.params[args_len]));
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
            Typespec* child = sema_astnode(s, astnode->exprstmt, NULL);
            if (child && sema_verify_isvalue(s, child, AT_DEFAULT_VALUE|AT_NORETURN, astnode->span)) {
                astnode->typespec = child;
                if (typespec_is_void(child) || child->kind == TS_NORETURN) {
                } else {
                    Msg msg = msg_with_span(
                        MSG_WARNING,
                        "expression value is not used",
                        astnode->span);
                    msg_emit(s, &msg);
                }
            }
            return astnode->typespec;
        } break;

        case ASTNODE_SYMBOL: {
            AstNode* node = sema_scope_retrieve(s, astnode->sym.identifier);
            if (node) {
                astnode->typespec = node->typespec;
                astnode->sym.ref = node;
            }
            return astnode->typespec;
        } break;

        case ASTNODE_BUILTIN_SYMBOL: {
            switch (astnode->bsym.kind) {
                case BS_u8:    astnode->typespec = predef_typespecs.u8_type; break;
                case BS_u16:   astnode->typespec = predef_typespecs.u16_type; break;
                case BS_u32:   astnode->typespec = predef_typespecs.u32_type; break;
                case BS_u64:   astnode->typespec = predef_typespecs.u64_type; break;
                case BS_i8:    astnode->typespec = predef_typespecs.i8_type; break;
                case BS_i16:   astnode->typespec = predef_typespecs.i16_type; break;
                case BS_i32:   astnode->typespec = predef_typespecs.i32_type; break;
                case BS_i64:   astnode->typespec = predef_typespecs.i64_type; break;
                case BS_bool:  astnode->typespec = predef_typespecs.bool_type; break;
                case BS_void:  astnode->typespec = predef_typespecs.void_type; break;
                case BS_noreturn:  astnode->typespec = predef_typespecs.noreturn_type; break;
                case BS_true:  astnode->typespec = predef_typespecs.bool_type->ty; break;
                case BS_false: astnode->typespec = predef_typespecs.bool_type->ty; break;
                default: assert(0);
            }
            return astnode->typespec;
        } break;

        case ASTNODE_ACCESS: {
            Typespec* left = sema_astnode(s, astnode->acc.left, NULL);
            if (left/*NOTE: No checking of `left` because types/modules are allowed as operands.*/) {
                assert(astnode->acc.right->kind == ASTNODE_SYMBOL);
                Token* right = astnode->acc.right->sym.identifier;
                AstNode* accessed_node = sema_access_field_from_type(s, left, right, astnode->short_span, false);
                astnode->typespec = accessed_node->typespec;
                astnode->acc.accessed = accessed_node;
                return astnode->typespec;
            } else return NULL;
        } break;

        case ASTNODE_GENERIC_TYPESPEC: {
            Typespec* left = sema_astnode(s, astnode->genty.left, NULL);
            Msg msg = msg_with_span(
                MSG_ERROR,
                "[internal] generics not implemented yet",
                astnode->span);
            msg_emit(s, &msg);
            // TODO: implement
        } break;

        case ASTNODE_IMPORT: {
            sema_scope_declare(s, astnode->import.name, astnode, astnode->import.arg->span);
        } break;

        case ASTNODE_IF: {
            // TODO: make small errors like wrong-cond-type not halt further analysis
            // of other astnodes (return a type). Maybe divide errors into further enumerations.
            bool error = false;

            bool first_br_val_valid = false;
            Typespec* first_br_val_type = NULL;
            Span first_br_val_span;

            bool else_req_if_value_br_error = false;

            if (sema_if_branch(
                s,
                astnode,
                astnode->iff.ifbr,
                &first_br_val_valid,
                &first_br_val_type,
                &first_br_val_span,
                &else_req_if_value_br_error)) error = true;
            bufloop(astnode->iff.elseifbr, i) {
                if (sema_if_branch(
                    s,
                    astnode,
                    astnode->iff.elseifbr[i],
                    &first_br_val_valid,
                    &first_br_val_type,
                    &first_br_val_span,
                    &else_req_if_value_br_error)) error = true;
            }
            if (astnode->iff.elsebr) {
                if (sema_if_branch(
                    s,
                    astnode,
                    astnode->iff.elsebr,
                    &first_br_val_valid,
                    &first_br_val_type,
                    &first_br_val_span,
                    &else_req_if_value_br_error)) error = true;
            } else if (!error) {
                // Here, the `if` branch is guaranteed to not yield to a non-void or non-noreturn type
                // because it errors if the else clause if missing in that situation.
                assert(typespec_is_void(astnode->typespec) || astnode->typespec->kind == TS_NORETURN);
                astnode->typespec = predef_typespecs.void_type->ty;
            }

            return error ? NULL : astnode->typespec;
        } break;

        case ASTNODE_WHILE: {
            AstNode* mainbody = astnode->whloop.mainbody;
            Typespec* mainbody_ty = sema_astnode(s, mainbody, NULL);
            astnode->typespec = predef_typespecs.void_type->ty;
            bool error = false;
            if (mainbody_ty && sema_verify_isvalue(s, mainbody_ty, AT_DEFAULT_VALUE|AT_NORETURN, mainbody->span)) {
                Span mainbody_val_span = mainbody->kind == ASTNODE_SCOPED_BLOCK
                                    ? (mainbody->blk.val ? mainbody->blk.val->span : mainbody->blk.rbrace->span)
                                    : (mainbody->span);
                if ((mainbody->kind == ASTNODE_SCOPED_BLOCK && mainbody->blk.val)
                    || (mainbody_ty->kind != TS_NORETURN && !typespec_is_void(mainbody_ty))) {
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        "`while` cannot yield a value",
                        mainbody_val_span);
                    msg_addl_thin(&msg, "use a `break` expression instead");
                    msg_emit(s, &msg);
                    error = true;
                }

                bufloop(astnode->whloop.breaks, i) {
                    AstNode* cur = astnode->whloop.breaks[i];
                    Typespec* break_ty = cur->brk.child ? cur->brk.child->typespec : predef_typespecs.void_type->ty;
                    if (break_ty) {
                        if (i != 0) {
                            Typespec* prev_break_ty = astnode->whloop.breaks[0]->brk.child ? astnode->whloop.breaks[0]->brk.child->typespec : predef_typespecs.void_type->ty;
                            TypeComparisonInfo cmpinfo = sema_are_types_equal(s, break_ty, prev_break_ty, cur->span);
                            if (cmpinfo.result == TC_OK) {
                                astnode->typespec = cmpinfo.final;
                            } else if (cmpinfo.result == TC_ERROR_HANDLED) {
                                error = true;
                            } else {
                                Msg msg = msg_with_span(
                                    MSG_ERROR,
                                    format_string_with_one_type("`break` type mismatch: this returns `%T`...", break_ty),
                                    cur->span);
                                msg_addl_fat(
                                    &msg,
                                    format_string_with_one_type("...while first `break` returns `%T`:", prev_break_ty),
                                    astnode->whloop.breaks[0]->span);
                                msg_emit(s, &msg);
                                error = true;
                            }
                        } else {
                            astnode->typespec = break_ty;
                        }
                    } else {
                        error = true;
                        continue;
                    }
                }
            } else error = true;
            if (error) return NULL; // early return if error
            error = false;

            AstNode* elsebody = astnode->whloop.elsebody;
            Typespec* elsebody_ty = NULL;
            if (elsebody) {
                elsebody_ty = sema_astnode(s, elsebody, NULL);
                if (elsebody_ty && sema_verify_isvalue(s, elsebody_ty, AT_DEFAULT_VALUE|AT_NORETURN, elsebody->span)) {
                    TypeComparisonInfo cmpinfo = sema_are_types_equal(s, elsebody_ty, astnode->typespec, elsebody->span);
                    if (cmpinfo.result == TC_OK) {
                        astnode->typespec = cmpinfo.final;
                    } else if (cmpinfo.result == TC_ERROR_HANDLED) {
                        error = true;
                    } else {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            format_string_with_one_type("`while`/`else` type mismatch: `else` returns `%T`...", elsebody_ty),
                            elsebody->kind == ASTNODE_SCOPED_BLOCK
                            ? (elsebody->blk.val ? elsebody->blk.val->span : elsebody->blk.rbrace->span)
                            : elsebody->span);
                        msg_addl_thin(&msg, format_string_with_one_type("...while `break` in `while` returns `%T`", astnode->typespec));
                        msg_emit(s, &msg);
                        error = true;
                    }
                } else error = true;
            } else if (!typespec_is_void(astnode->typespec)) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "`else` clause is required when `break` returns a value",
                    astnode->span);
                msg_addl_fat(&msg, "`break` returns a value here:", astnode->whloop.breaks[0]->span);
                msg_emit(s, &msg);
                error = true;
            }
            return error ? NULL : astnode->typespec;
        } break;

        case ASTNODE_BREAK: {
            if (astnode->brk.child) {
                Typespec* child = sema_astnode(s, astnode->brk.child, NULL);
                if (child && sema_verify_isvalue(s, child, AT_DEFAULT_VALUE, astnode->brk.child->span)) {
                    astnode->typespec = predef_typespecs.noreturn_type->ty;
                } else return NULL;
            } else {
                astnode->typespec = predef_typespecs.noreturn_type->ty;
            }
            return astnode->typespec;
        } break;

        case ASTNODE_CONTINUE: {
            astnode->typespec = predef_typespecs.noreturn_type->ty;
            return astnode->typespec;
        } break;

        case ASTNODE_RETURN: {
            if (!s->current_func) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "`return` used outside of a function",
                    astnode->span);
                msg_emit(s, &msg);
                return NULL;
            }

            astnode->ret.ref = s->current_func;
            Typespec* func_ret = astnode->ret.ref->typespec->func.ret_typespec;
            if (astnode->ret.child) {
                Typespec* child = sema_astnode(s, astnode->ret.child, NULL);
                if (child && sema_verify_isvalue(s, child, AT_DEFAULT_VALUE, astnode->ret.child->span)) {
                    if (sema_check_types_equal(s, child, func_ret, false, astnode->ret.child->span)) {
                    } else return NULL;
                } else return NULL;
            }
            astnode->typespec = predef_typespecs.noreturn_type->ty;
            return astnode->typespec;
        } break;

        case ASTNODE_VARIABLE_DECL: {
            if (!astnode->vard.stack) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "[internal] global variables not implemented",
                    astnode->span);
                msg_emit(s, &msg);
                return NULL;
            }

            bufpush(s->current_func->funcdef.locals, astnode);
            bool error = false;
            if (!sema_check_variable_type(s, astnode)) error = true;
            Typespec* initializer = NULL;
            if (astnode->vard.initializer) {
                initializer = sema_astnode(s, astnode->vard.initializer, astnode->typespec);
                if (initializer && sema_verify_isvalue(s, initializer, AT_RUNTIME|AT_COMPTIME, astnode->vard.initializer->span)) {
                } else error = true;
            }
            if (!sema_declare_variable(s, astnode)) error = true;

            if (error) return NULL;
            error = false;

            Typespec* annotated = astnode->typespec;
            if (annotated && initializer) {
                if (sema_check_types_equal(s, initializer, annotated, false, astnode->vard.equal->span)) {
                } else error = true;
            } else if (initializer) {
                if (typespec_is_unsized_integer(initializer) && !astnode->vard.immutable) {
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        format_string_with_one_type("mutable integer must have a sized integer type, got `%T`", initializer),
                        astnode->short_span);
                    msg_emit(s, &msg);
                    error = true;
                } else astnode->typespec = initializer;
            } else if (!annotated && !initializer) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "no type annotation/initializer provided for declaration",
                    astnode->span);
                msg_emit(s, &msg);
            }

            return error ? NULL : astnode->typespec;
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
            sema_astnode(s, s->srcfile->astnodes[j], NULL);
        }
        if (s->error) error = true;
    }
    if (error) return true;

    return false;
}
