#include "type.h"
#include "buf.h"
#include "ast.h"

Typespec* typespec_prim_new(PrimKind prim) {
    Typespec* ty = alloc_obj(Typespec);
    ty->kind = TS_PRIM;
    ty->prim = prim;
    return ty;
}

Typespec* typespec_ptr_new(bool immutable, Typespec* child) {
    Typespec* ty = alloc_obj(Typespec);
    ty->kind = TS_PTR;
    ty->ptr.immutable = immutable;
    ty->ptr.child = child;
    return ty;
}

Typespec* typespec_func_new(Typespec** params, Typespec* ret_typespec) {
    Typespec* ty = alloc_obj(Typespec);
    ty->kind = TS_FUNC;
    ty->func.params = params;
    ty->func.ret_typespec = ret_typespec;
    return ty;
}

Typespec* typespec_struct_new(struct AstNode* astnode) {
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

static char* primkind_tostring(PrimKind kind) {
    switch (kind) {
        case PRIM_u8: return "u8";
        case PRIM_u16: return "u16";
        case PRIM_u32: return "u32";
        case PRIM_u64: return "u64";
        case PRIM_i8: return "i8";
        case PRIM_i16: return "i16";
        case PRIM_i32: return "i32";
        case PRIM_i64: return "i64";
        case PRIM_void: return "void";
    }
}

static char* tostring(Typespec* ty) {
    switch (ty->kind) {
        case TS_PRIM: {
            return primkind_tostring(ty->prim);
        } break;

        case TS_PTR: {
            char* buf = NULL;
            bufpush(buf, '*');
            if (ty->ptr.immutable) bufstrexpandpush(buf, "imm ");
            bufstrexpandpush(buf, tostring(ty->ptr.child));
            return buf;
        } break;

        case TS_FUNC: {
            char* buf = NULL;
            bufstrexpandpush(buf, "fn (");
            bufloop(ty->func.params, i) {
                bufstrexpandpush(buf, tostring(ty->func.params[i]));
                if (i != buflen(ty->func.params)-1) bufstrexpandpush(buf, ", ");
            }
            bufstrexpandpush(buf, ") ");
            bufstrexpandpush(buf, tostring(ty->func.ret_typespec));
            return buf;
        } break;

        case TS_STRUCT: {
            return token_tostring(ty->agg->strct.identifier);
        } break;

        case TS_TYPE: {
            return tostring(ty->ty);
        } break;
    }
    assert(0);
    return NULL;
}

char* typespec_tostring(Typespec* ty) {
    char* buf = NULL;
    bufstrexpandpush(buf, tostring(ty));
    return buf;
}
