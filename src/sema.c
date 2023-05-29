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

static void sema_scope_declare(SemaCtx* s, Token* identifier, Typespec* value) {
    bool found = false;
    bufrevloop(s->scopebuf, si) {
        bufloop(s->scopebuf[si].decls, i) {
            Token* tok = s->scopebuf[si].decls[i].key;
            if (are_token_lexemes_equal(tok, identifier)) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    "symbol is redeclared",
                    identifier->span);
                msg_addl_fat(&msg, "previous declaration here", tok->span);
                msg_emit(s, &msg);
                found = true;
                break;
            }
        }
        if (found) break;
    }

    if (!found) bufpush(sema_curscope(s)->decls, (TokenTypespecTup){
        .key = identifier,
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
            Token* tok = s->scopebuf[si].decls[i].key;
            if (are_token_lexemes_equal(tok, identifier)) {
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

static void sema_top_level_decls_prec1(SemaCtx* s, AstNode* astnode) {
    switch (astnode->kind) {
        case ASTNODE_STRUCT: {
            Typespec* ty = typespec_type_new(typespec_struct_new(astnode));
            astnode->typespec = ty;
            sema_scope_declare(s, astnode->strct.identifier, ty);
        } break;
    }
}

static void sema_top_level_decls_prec2(SemaCtx* s, AstNode* astnode) {
    switch (astnode->kind) {
        case ASTNODE_VARIABLE_DECL: {
            Typespec* ty = sema_astnode(s, astnode->vard.typespec);
            if (ty) {
                if (sema_assert_is_type(s, ty, astnode->vard.typespec))
                    astnode->typespec = ty->ty;
            } else return;
            sema_scope_declare(s, astnode->vard.identifier, astnode->typespec);
        } break;

        case ASTNODE_FUNCTION_DEF: {
            bool error = false;
            Typespec** params_ty = NULL;
            AstNodeFunctionHeader* header = &astnode->funcdef.header->funch;
            bufloop(header->params, i) {
                Typespec* param_ty = sema_astnode(s, header->params[i]->field.value);
                if (param_ty) {
                    if (!sema_assert_is_type(s, param_ty, header->params[i]->field.value)) error = true;
                } else error = true;
                bufpush(params_ty, param_ty);
            }

            Typespec* ret_ty = sema_astnode(s, header->ret_typespec);
            if (ret_ty) {
                if (!sema_assert_is_type(s, ret_ty, header->ret_typespec)) error = true;
            } else error = true;

            if (!error) astnode->typespec = typespec_func_new(params_ty, ret_ty);
            sema_scope_declare(s, header->identifier, astnode->typespec);
        } break;
    }
}

static Typespec* sema_typespec(SemaCtx* s, AstNode* astnode) {
    switch (astnode->kind) {
        case ASTNODE_TYPESPEC_PTR: {
            Typespec* child = sema_astnode(s, astnode->typeptr.child);
            if (child) {
                if (sema_assert_is_type(s, child, astnode->typeptr.child)) {
                    astnode->typespec = typespec_ptr_new(astnode->typeptr.immutable, child);
                    return astnode->typespec;
                } else return NULL;
            } else return NULL;
        } break;
    }
    return NULL;
}

static Typespec* sema_astnode(SemaCtx* s, AstNode* astnode) {
    switch (astnode->kind) {
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
                    char* error_fmt = format_string("expected %d argument(s), found %d", params_len, args_len);
                    if (args_len < params_len) {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            error_fmt,
                            astnode->funcc.rparen->span);
                        char* fmt = NULL;
                        bufstrexpandpush(fmt, "expected argument of type `");
                        bufstrexpandpush(fmt, typespec_tostring(callee_ty->func.params[args_len]));
                        bufpush(fmt, '`');
                        msg_addl_thin(&msg, fmt);
                        msg_emit(s, &msg);
                    } else if (args_len > params_len) {
                        Msg msg = msg_with_span(
                            MSG_ERROR,
                            error_fmt,
                            span_from_two(astnode->funcc.args[params_len]->span, astnode->funcc.args[args_len-1]->span));
                        msg_emit(s, &msg);
                    }
                } else {
                    char* fmt = NULL;
                    bufstrexpandpush(fmt, "attempting to call type `");
                    bufstrexpandpush(fmt, typespec_tostring(callee_ty));
                    bufpush(fmt, '`');
                    Msg msg = msg_with_span(
                        MSG_ERROR,
                        fmt,
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
                if (left->kind == TS_STRUCT) {
                    AstNode** fields = left->agg->strct.fields;
                    bufloop(fields, i) {
                        if (are_token_lexemes_equal(
                                fields[i]->field.key,
                                astnode->acc.right->sym.identifier)) {
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
                }
            } else return NULL;
        } break;

        case ASTNODE_GENERIC_TYPESPEC: {
            Typespec* left = sema_astnode(s, astnode->genty.left);
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
