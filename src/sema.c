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

Typespec* typespec_struct_new(AstNode* astnode) {
    Typespec* ty = alloc_obj(Typespec);
    ty->kind = TS_STRUCT;
    ty->agg = astnode;
    return ty;
}

Typespec* typespec_type_new(Typespec* typespec) {
    Typespec* ty = alloc_obj(Typespec);
    ty->kind = TS_TYPE;
    ty->ty = typespec;
    return ty;
}

static Typespec* sema_scope_retrieve(SemaCtx* s, Token* identifier) {
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

static bool sema_assert_is_type(SemaCtx* s, Typespec* ty, AstNode* astnode) {
    assert(ty != NULL);
    if (ty->kind == TS_TYPE) {
        return true;
    }

    Msg msg = msg_with_span(
        MSG_ERROR,
        "expected type, got value",
        astnode->span);
    msg_emit(s, &msg);
    return false;
}

static void sema_top_level_decls(SemaCtx* s, AstNode* astnode) {
    switch (astnode->kind) {
        case ASTNODE_STRUCT: {
            Typespec* ty = typespec_type_new(typespec_struct_new(astnode));
            astnode->typespec = ty;
            sema_scope_declare(s, astnode->strct.identifier, ty);
        } break;

        case ASTNODE_VARIABLE_DECL: {
            Typespec* ty = sema_astnode(s, astnode->vard.typespec);
            if (ty) {
                if (sema_assert_is_type(s, ty, astnode->vard.typespec)) astnode->typespec = ty->ty;
                sema_scope_declare(s, astnode->vard.identifier, astnode->typespec);
            } else return;
        } break;

        case ASTNODE_FUNCTION_DEF: {
            sema_scope_declare(s, astnode->funcdef.header->funch.identifier, NULL);
        } break;
    }
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
                } else error = true;
            }
            return error ? NULL : astnode->typespec;
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
            sema_top_level_decls(s, s->srcfile->astnodes[j]);
        }
        if (s->error) error = true;
    }
    if (error) return true;

    /* bufloop(sema_ctxs, i) { */
    /*     SemaCtx* s = &sema_ctxs[i]; */
    /*     bufloop(s->srcfile->astnodes, j) { */
    /*         sema_aggregate_defs(s, s->srcfile->astnodes[j]); */
    /*     } */
    /*     if (s->error) error = true; */
    /* } */
    /* if (error) return true; */

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
