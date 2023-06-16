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

static ScopeRetrieveInfo sema_scope_retrieve(SemaCtx* s, Token* identifier) {
    if (is_token_lexeme(identifier, "u8"))        return (ScopeRetrieveInfo){ .type = predef_typespecs.u8_type, .ref = NULL };
    else if (is_token_lexeme(identifier, "u16"))  return (ScopeRetrieveInfo){ .type = predef_typespecs.u16_type, .ref = NULL };
    else if (is_token_lexeme(identifier, "u32"))  return (ScopeRetrieveInfo){ .type = predef_typespecs.u32_type, .ref = NULL };
    else if (is_token_lexeme(identifier, "u64"))  return (ScopeRetrieveInfo){ .type = predef_typespecs.u64_type, .ref = NULL };
    else if (is_token_lexeme(identifier, "i8"))   return (ScopeRetrieveInfo){ .type = predef_typespecs.i8_type, .ref = NULL };
    else if (is_token_lexeme(identifier, "i16"))  return (ScopeRetrieveInfo){ .type = predef_typespecs.i16_type, .ref = NULL };
    else if (is_token_lexeme(identifier, "i32"))  return (ScopeRetrieveInfo){ .type = predef_typespecs.i32_type, .ref = NULL };
    else if (is_token_lexeme(identifier, "i64"))  return (ScopeRetrieveInfo){ .type = predef_typespecs.i64_type, .ref = NULL };
    else if (is_token_lexeme(identifier, "void")) return (ScopeRetrieveInfo){ .type = predef_typespecs.void_type, .ref = NULL };

    bufrevloop(s->scopebuf, si) {
        bufloop(s->scopebuf[si].decls, i) {
            char* defined = s->scopebuf[si].decls[i].key;
            if (is_token_lexeme(identifier, defined)) {
                return (ScopeRetrieveInfo){ .type = s->scopebuf[si].decls[i].value->typespec, .ref = s->scopebuf[si].decls[i].value };
            }
        }
    }
    Msg msg = msg_with_span(
        MSG_ERROR,
        "undefined symbol",
        identifier->span);
    msg_emit(s, &msg);
    return (ScopeRetrieveInfo){ .type = NULL, .ref = NULL };
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

static bool type_comparison_ok(TypeComparisonResult cmpresult) {
    if (cmpresult != TC_OK && cmpresult != TC_ERROR_HANDLED) return false;
    return true;
}

static TypeComparisonResult sema_are_types_equal(SemaCtx* s, Typespec* from, Typespec* to, Span error, bool exact) {
    if (from->kind == to->kind) {
        switch (from->kind) {
            case TS_PRIM: {
                if (from->prim.kind == to->prim.kind) return TC_OK;
                else if (from->prim.kind == PRIM_void || to->prim.kind == PRIM_void) return TC_MISMATCH;

                if (from->prim.kind == PRIM_comptime_integer || to->prim.kind == PRIM_comptime_integer) {
                    if (from->prim.kind == PRIM_comptime_integer && to->prim.kind == PRIM_comptime_integer) return TC_OK;
                    else if (typespec_is_integer(to) || typespec_is_integer(from)) {
                        Typespec* sized = from->prim.kind == PRIM_comptime_integer ? to : from;
                        Typespec* unsized = from->prim.kind == PRIM_comptime_integer ? from : to;

                        if (bigint_fits(
                            &unsized->prim.comptime_integer,
                            typespec_get_bytes(sized),
                            typespec_is_signed(sized))) {
                            return TC_OK;
                        } else {
                            Msg msg = msg_with_span(
                                MSG_ERROR,
                                format_string_with_one_type(
                                    "type `%T` cannot fit integer `%s`",
                                    sized,
                                    bigint_tostring(&unsized->prim.comptime_integer)),
                                error);
                            msg_addl_thin(
                                &msg,
                                format_string_with_one_type(
                                    "range of type `%T` is [%s, %s]",
                                    sized,
                                    typespec_integer_get_min_value(sized),
                                    typespec_integer_get_max_value(sized)));
                            msg_emit(s, &msg);
                            return TC_ERROR_HANDLED;
                        }
                    } else return TC_MISMATCH;
                } else if (typespec_is_signed(from) == typespec_is_signed(to)) {
                    if (exact) {
                        if (typespec_get_bytes(from) == typespec_get_bytes(to)) return TC_OK;
                        else return TC_MISMATCH;
                    } else {
                        if (typespec_get_bytes(from) <= typespec_get_bytes(to)) return TC_OK;
                        else return TC_MISMATCH;
                    }
                } else return TC_MISMATCH;
            } break;

            case TS_PTR: {
                if (!from->ptr.immutable || to->ptr.immutable) {
                    return sema_are_types_equal(s, from->ptr.child, to->ptr.child, error, true);
                } else return TC_CONST;
            } break;

            case TS_MULTIPTR: {
                if (!from->mulptr.immutable || to->mulptr.immutable) {
                    return sema_are_types_equal(s, from->mulptr.child, to->mulptr.child, error, true);
                } else return TC_CONST;
            } break;

            case TS_SLICE: {
                if (!from->slice.immutable || to->slice.immutable) {
                    return sema_are_types_equal(s, from->slice.child, to->slice.child, error, true);
                } else return TC_CONST;
            } break;

            case TS_ARRAY: {
                if (bigint_cmp(&from->array.size->prim.comptime_integer, &to->array.size->prim.comptime_integer) == 0) {
                    return sema_are_types_equal(s, from->array.child, to->array.child, error, true);
                } else return TC_MISMATCH;
            } break;

            case TS_STRUCT: {
                if (from->agg == to->agg) return TC_OK;
                else return TC_MISMATCH;
            } break;

            case TS_TYPE: {
                return sema_are_types_equal(s, from->ty, to->ty, error, false);
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
            return sema_are_types_equal(s, from->ptr.child->array.child, to_child, error, true);
        } else return TC_CONST;
    } else {
        return TC_MISMATCH;
    }

    assert(0);
    return TC_MISMATCH;
}

static bool sema_check_types_equal(SemaCtx* s, Typespec* from, Typespec* to, Span error) {
    TypeComparisonResult cmpresult = sema_are_types_equal(s, from, to, error, false);
    if (!type_comparison_ok(cmpresult)) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            format_string_with_two_types("expected type `%T`, got `%T`", to, from),
            error);
        if (cmpresult == TC_CONST) {
            msg_addl_thin(&msg, "type mismatch due to change in immutability");
        }
        msg_emit(s, &msg);
    }
    return cmpresult == TC_OK ? true : false;
}

typedef enum {
    AT_VALUE,
    AT_VALUE_SIZED,
    AT_TYPE,
    AT_MODULE,
} AcceptTypespecKind;

static const char* accept_typespec_kind_tostring(AcceptTypespecKind accept) {
    switch (accept) {
        case AT_VALUE: return "value";
        case AT_VALUE_SIZED: return "value";
        case AT_TYPE: return "type";
        case AT_MODULE: return "module";
        default: assert(0);
    }
}

static bool sema_verify_typespec(SemaCtx* s, Typespec* ty, AcceptTypespecKind accept, Span error) {
    AcceptTypespecKind given_ty;
    if (ty->kind == TS_TYPE)        given_ty = AT_TYPE;
    else if (ty->kind == TS_MODULE) given_ty = AT_MODULE;
    else                            given_ty = typespec_is_sized(ty) && accept == AT_VALUE_SIZED
                                               ? AT_VALUE_SIZED
                                               : AT_VALUE;

    if (given_ty == accept) {
        return true;
    } else if (accept == AT_VALUE_SIZED && given_ty == AT_VALUE) {
        Msg msg = msg_with_span(
            MSG_ERROR,
            format_string_with_one_type("expected sized value, got `%T`", ty),
            error);
        msg_emit(s, &msg);
        return false;
    } else {
        const char* expected = accept_typespec_kind_tostring(accept);
        const char* given = accept_typespec_kind_tostring(given_ty);
        Msg msg = msg_with_span(
            MSG_ERROR,
            format_string("expected %s, got %s", expected, given),
            error);
        msg_emit(s, &msg);
        return false;
    }
}

static bool sema_check_is_lvalue(SemaCtx* s, AstNode* astnode) {
    if (astnode->kind == ASTNODE_ACCESS ||
        astnode->kind == ASTNODE_DEREF ||
        astnode->kind == ASTNODE_INDEX ||
        astnode->kind == ASTNODE_SYMBOL) {
        if (astnode->kind == ASTNODE_SYMBOL
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
        if (ty && sema_verify_typespec(s, ty, AT_TYPE, astnode->vard.typespec->span)) {
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
                Typespec* param_ty = sema_astnode(s, header->params[i]->paramd.typespec, NULL);
                if (param_ty && sema_verify_typespec(s, param_ty, AT_TYPE, header->params[i]->paramd.typespec->span)) {
                    bufpush(params_ty, param_ty->ty);
                } else error = true;
            }

            Typespec* ret_ty = sema_astnode(s, header->ret_typespec, NULL);
            if (ret_ty && sema_verify_typespec(s, ret_ty, AT_TYPE, header->ret_typespec->span)) {
            } else error = true;

            if (!error) astnode->typespec = typespec_func_new(params_ty, ret_ty->ty);
            sema_scope_declare(s, header->name, astnode, header->identifier->span);
        } break;

        case ASTNODE_STRUCT: {
            bool error = false;
            bufloop(astnode->strct.fields, i) {
                AstNode* fieldnode = astnode->strct.fields[i];
                Typespec* field_ty = sema_astnode(s, fieldnode->field.value, NULL);
                if (field_ty && sema_verify_typespec(s, field_ty, AT_TYPE, fieldnode->field.value->span)) {
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

static Typespec* sema_access_field_from_type(SemaCtx* s, Typespec* ty, Token* key, Span error, bool derefed) {
    if (ty->kind == TS_STRUCT) {
        AstNode** fields = ty->agg->strct.fields;
        bufloop(fields, i) {
            if (are_token_lexemes_equal(
                    fields[i]->field.key,
                    key)) {
                return fields[i]->field.value->typespec->ty;
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
                return astnodes[i]->typespec;
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
        if (bigint_cmp(&ty->array.size->prim.comptime_integer, &BIGINT_ZERO) != 0) {
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

static Typespec* sema_astnode(SemaCtx* s, AstNode* astnode, Typespec* target) {
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

        case ASTNODE_STRING_LITERAL: {
            Token* lit = astnode->strl.token;
            bigint lit_size = bigint_new_u64((lit->span.end-1) - (lit->span.start+1));
            astnode->typespec = typespec_ptr_new(
                true,
                typespec_array_new(
                    typespec_comptime_integer_new(lit_size),
                    predef_typespecs.u8_type->ty));
            return astnode->typespec;
        } break;

        case ASTNODE_ARRAY_LITERAL: {
            Typespec* elem_annotated = NULL;
            if (astnode->arrayl.elem_type) {
                Typespec* typeof_annotated = sema_astnode(s, astnode->arrayl.elem_type, NULL);
                if (typeof_annotated && sema_verify_typespec(s, typeof_annotated, AT_TYPE, astnode->arrayl.elem_type->span)) {
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

                Typespec* final_elem_type = NULL;
                if (elem_annotated) final_elem_type = elem_annotated;
                else final_elem_type = sema_astnode(s, elems[0], NULL);

                bool unsized_elems = false;

                for (usize i = elem_annotated ? 0 : 1; i < buflen(elems); i++) {
                    Typespec* cur_elem_type = sema_astnode(s, elems[i], final_elem_type);
                    if (cur_elem_type && sema_verify_typespec(s, cur_elem_type, AT_VALUE, elems[i]->span)) {
                        if (!typespec_is_sized(cur_elem_type)) unsized_elems = true;
                        if (sema_check_types_equal(s, cur_elem_type, final_elem_type, elems[i]->span)) {
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
                astnode->typespec = typespec_array_new(typespec_comptime_integer_new(size), final_elem_type);
                return astnode->typespec;
            } else {
                bigint zero = bigint_new_u64(0);
                astnode->typespec = typespec_array_new(typespec_comptime_integer_new(zero), elem_annotated);
                return astnode->typespec;
            }
        } break;

        case ASTNODE_UNOP: {
            Typespec* child = sema_astnode(s, astnode->unop.child, NULL);
            switch (astnode->unop.kind) {
                case UNOP_NEG: {
                    if (child && sema_verify_typespec(s, child, AT_VALUE, astnode->unop.child->span)) {
                        if (typespec_is_integer(child)) {
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
                        } else if (typespec_is_comptime_integer(child)) {
                            bigint new = bigint_new();
                            bigint_copy(&new, &child->prim.comptime_integer);
                            bigint_neg(&new);
                            if (sema_check_bigint_overflow(s, &new, astnode->span)) return NULL;

                            Typespec* tynew = typespec_comptime_integer_new(new);
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

                case UNOP_ADDR: {
                    if (child && sema_verify_typespec(s, child, AT_VALUE_SIZED, astnode->unop.child->span)) {
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
            if (child && sema_verify_typespec(s, child, AT_VALUE_SIZED, astnode->deref.child->span)) {
                if (child->kind == TS_PTR) {
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
            if (left && sema_verify_typespec(s, left, AT_VALUE, astnode->idx.left->span)) {
                Typespec* after_idx = sema_index_type(s, left, astnode->idx.left->span, false);
                if (after_idx) astnode->typespec = after_idx;
                else error = true;
            } else error = true;

            if (idx && sema_verify_typespec(s, idx, AT_VALUE, astnode->idx.idx->span)) {
                if (typespec_is_integer(idx)) {
                    if (typespec_is_signed(idx)) {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            format_string_with_one_type("index requires an unsigned integer, got `%T`", idx),
                            astnode->idx.idx->span);
                        msg_emit(s, &msg);
                        error = true;
                    }
                } else if (typespec_is_comptime_integer(idx)) {
                    if (idx->prim.comptime_integer.neg) {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            format_string("index requires a non-negative integer, got `%s`", bigint_tostring(&idx->prim.comptime_integer)),
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

        case ASTNODE_BINOP: {
            Typespec* left_ty = sema_astnode(s, astnode->binop.left, NULL);
            Typespec* right_ty = sema_astnode(s, astnode->binop.right, NULL);
            if (left_ty && right_ty) {
                bool left_valid = sema_verify_typespec(s, left_ty, AT_VALUE, astnode->binop.left->span);
                bool right_valid = sema_verify_typespec(s, right_ty, AT_VALUE, astnode->binop.right->span);
                if (left_valid && right_valid) {
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
                            astnode->short_span); \
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
                                    astnode->short_span);
                                msg_emit(s, &msg);
                                return NULL;
                            }
                        } else if (left_is_comptime_integer && right_is_comptime_integer) {
                            bigint new = bigint_new();
                            bigint_copy(&new, &left_ty->prim.comptime_integer);
                            switch (astnode->binop.kind) {
                                case BINOP_ADD:
                                    bigint_add(&new, &right_ty->prim.comptime_integer); break;
                                case BINOP_SUB:
                                    bigint_sub(&new, &right_ty->prim.comptime_integer); break;
                                default: assert(0);
                            }

                            if (sema_check_bigint_overflow(s, &new, astnode->span)) return NULL;

                            Typespec* tynew = typespec_comptime_integer_new(new);
                            astnode->typespec = tynew;
                            return tynew;
                        } else {
                            Typespec* comptime_integer = left_ty->prim.kind == PRIM_comptime_integer ? left_ty : right_ty;
                            Typespec* fixed_integer = left_ty->prim.kind == PRIM_comptime_integer ? right_ty : left_ty;
                            bool is_left_comptime_integer = left_ty->prim.kind == PRIM_comptime_integer;

                            if (bigint_fits(
                                &comptime_integer->prim.comptime_integer,
                                typespec_get_bytes(fixed_integer),
                                typespec_is_signed(fixed_integer))) {
                                astnode->typespec = fixed_integer;
                                return astnode->typespec;
                            } else {
                                Msg msg = msg_with_span(
                                    MSG_ERROR,
                                    format_string_with_one_type(
                                        "%s operand `%s` cannot fit in %s operand's type `%T`",
                                        fixed_integer,
                                        is_left_comptime_integer ? "left" : "right",
                                        bigint_tostring(&comptime_integer->prim.comptime_integer),
                                        is_left_comptime_integer ? "right" : "left"),
                                    astnode->short_span);
                                // TODO: add range help note
                                msg_emit(s, &msg);
                                return NULL;
                            }
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
                }
            } else return NULL;
        } break;

        case ASTNODE_ASSIGN: {
            Typespec* left = sema_astnode(s, astnode->assign.left, NULL);
            Typespec* right = sema_astnode(s, astnode->assign.right, left);
            bool error = false;

            if (left && sema_verify_typespec(s, left, AT_VALUE, astnode->assign.left->span)) {
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

                if (right && sema_verify_typespec(s, right, AT_VALUE, astnode->assign.right->span)) {
                    if (sema_check_types_equal(s, right, left, astnode->short_span)) {
                        astnode->typespec = predef_typespecs.void_type;
                    } else error = true;
                }
            } else error = true;

            return error ? NULL : astnode->typespec;
        } break;

        case ASTNODE_TYPESPEC_PTR: {
            Typespec* child = sema_astnode(s, astnode->typeptr.child, NULL);
            if (child && sema_verify_typespec(s, child, AT_TYPE, astnode->typeptr.child->span)) {
                astnode->typespec = typespec_type_new(typespec_ptr_new(astnode->typeptr.immutable, child->ty));
                return astnode->typespec;
            } else return NULL;
        } break;

        case ASTNODE_TYPESPEC_MULTIPTR: {
            Typespec* child = sema_astnode(s, astnode->typemulptr.child, NULL);
            if (child && sema_verify_typespec(s, child, AT_TYPE, astnode->typemulptr.child->span)) {
                astnode->typespec = typespec_type_new(typespec_multiptr_new(astnode->typemulptr.immutable, child->ty));
                return astnode->typespec;
            } else return NULL;
        } break;

        case ASTNODE_TYPESPEC_SLICE: {
            Typespec* child = sema_astnode(s, astnode->typeslice.child, NULL);
            if (child && sema_verify_typespec(s, child, AT_TYPE, astnode->typeslice.child->span)) {
                astnode->typespec = typespec_type_new(typespec_slice_new(astnode->typeslice.immutable, child->ty));
                return astnode->typespec;
            } else return NULL;
        } break;

        case ASTNODE_TYPESPEC_ARRAY: {
            Typespec* child = sema_astnode(s, astnode->typearray.child, NULL);
            Typespec* size = sema_astnode(s, astnode->typearray.size, NULL);
            bool error = false;

            if (child && sema_verify_typespec(s, child, AT_TYPE, astnode->typearray.child->span)) {
            } else error = true;
            if (size && sema_verify_typespec(s, size, AT_VALUE, astnode->typearray.size->span)) {
                if (typespec_is_comptime_integer(size)) {
                    if (size->prim.comptime_integer.neg) {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            format_string(
                                "array size should be a non-negative integer, got `%s`",
                                bigint_tostring(&size->prim.comptime_integer)),
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
            if (!astnode->funcdef.global) {
                sema_top_level_decls_prec2(s, astnode);
            }
            sema_astnode(s, astnode->funcdef.body, astnode->funcdef.header->funch.ret_typespec->typespec->ty);
        } break;

        case ASTNODE_SCOPED_BLOCK: {
            bool error = false;
            bufloop(astnode->blk.stmts, i) {
                if (!sema_astnode(s, astnode->blk.stmts[i], target)) error = true;
            }
            return error ? NULL : NULL;
        } break;

        case ASTNODE_FUNCTION_CALL: {
            Typespec* callee_ty = sema_astnode(s, astnode->funcc.callee, NULL);
            if (callee_ty && sema_verify_typespec(s, callee_ty, AT_VALUE, astnode->funcc.callee->span)) {
                if (callee_ty->kind == TS_FUNC) {
                    usize params_len = buflen(callee_ty->func.params);
                    usize args_len = buflen(astnode->funcc.args);
                    if (args_len == params_len) {
                        bool error = false;
                        Typespec** args_ty = NULL;
                        bufloop(astnode->funcc.args, i) {
                            Typespec* arg_ty = sema_astnode(s, astnode->funcc.args[i], callee_ty->func.params[i]);
                            if (arg_ty && sema_verify_typespec(s, arg_ty, AT_VALUE, astnode->funcc.args[i]->span)) {
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
            sema_astnode(s, astnode->exprstmt, NULL);
        } break;

        case ASTNODE_SYMBOL: {
            ScopeRetrieveInfo info = sema_scope_retrieve(s, astnode->sym.identifier);
            astnode->typespec = info.type;
            astnode->sym.ref = info.ref;
            return astnode->typespec;
        } break;

        case ASTNODE_ACCESS: {
            Typespec* left = sema_astnode(s, astnode->acc.left, NULL);
            if (left/*NOTE: No checking of `left` because types/mmodules are allowed as operands.*/) {
                assert(astnode->acc.right->kind == ASTNODE_SYMBOL);
                Token* right = astnode->acc.right->sym.identifier;
                astnode->typespec = sema_access_field_from_type(s, left, right, astnode->short_span, false);
                return astnode->typespec;
            } else return NULL;
        } break;

        case ASTNODE_GENERIC_TYPESPEC: {
            Typespec* left = sema_astnode(s, astnode->genty.left, NULL);
            // TODO: implement
        } break;

        case ASTNODE_IMPORT: {
            sema_scope_declare(s, astnode->import.name, astnode, astnode->import.arg->span);
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

            bool error = false;
            if (!sema_check_variable_type(s, astnode)) error = true;
            Typespec* initializer = sema_astnode(s, astnode->vard.initializer, astnode->typespec);
            if (initializer && sema_verify_typespec(s, initializer, AT_VALUE, astnode->vard.initializer->span)) {
            } else error = true;
            if (!sema_declare_variable(s, astnode)) error = true;

            if (error) return NULL;
            error = false;

            Typespec* annotated = astnode->typespec;
            if (annotated && initializer) {
                if (sema_check_types_equal(s, initializer, annotated, astnode->vard.equal->span)) {
                } else error = true;
            } else if (initializer) {
                if (typespec_is_comptime_integer(initializer) && !astnode->vard.immutable) {
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        format_string_with_one_type("integer variable must have a sized type, got `%T`", initializer),
                        astnode->short_span);
                    msg_emit(s, &msg);
                    error = true;
                } else astnode->typespec = initializer;
            } else error = true;

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
