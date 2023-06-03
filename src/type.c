#include "type.h"
#include "buf.h"
#include "ast.h"

Typespec* typespec_prim_new(PrimKind kind) {
    Typespec* ty = alloc_obj(Typespec);
    ty->kind = TS_PRIM;
    ty->prim.kind = kind;
    return ty;
}

Typespec* typespec_comptime_integer_new(bigint val) {
    Typespec* ty = alloc_obj(Typespec);
    ty->kind = TS_PRIM;
    ty->prim.kind = PRIM_comptime_integer;
    ty->prim.comptime_integer = val;
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
        case PRIM_comptime_integer: return "{integer}";
    }
}

static char* tostring(Typespec* ty) {
    switch (ty->kind) {
        case TS_PRIM: {
            return primkind_tostring(ty->prim.kind);
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

bool typespec_is_integer(Typespec* ty) {
    if (ty->kind == TS_TYPE) return typespec_is_integer(ty->ty);
    assert(ty->kind == TS_PRIM);
    switch (ty->prim.kind) {
        case PRIM_u8:
        case PRIM_i8:
        case PRIM_u16:
        case PRIM_i16:
        case PRIM_u32:
        case PRIM_i32:
        case PRIM_u64:
        case PRIM_i64:
            return true;
        default:
            return false;
    }
}

bool typespec_is_comptime_integer(Typespec* ty) {
    return ty->kind == TS_PRIM && ty->prim.kind == PRIM_comptime_integer;
}

bool typespec_is_signed(Typespec* ty) {
    if (ty->kind == TS_TYPE) return typespec_is_signed(ty->ty);
    assert(ty->kind == TS_PRIM);
    switch (ty->prim.kind) {
        case PRIM_i8:
        case PRIM_i16:
        case PRIM_i32:
        case PRIM_i64:
            return true;
        case PRIM_u8:
        case PRIM_u16:
        case PRIM_u32:
        case PRIM_u64:
            return false;
    }
    assert(0);
    return false;
}

u32 typespec_get_bytes(Typespec* ty) {
    if (ty->kind == TS_TYPE) return typespec_get_bytes(ty->ty);
    switch (ty->kind) {
        case TS_PRIM: {
            switch (ty->prim.kind) {
            case PRIM_u8:
            case PRIM_i8:
                return 1;
            case PRIM_u16:
            case PRIM_i16:
                return 2;
            case PRIM_u32:
            case PRIM_i32:
                return 4;
            case PRIM_u64:
            case PRIM_i64:
                return 8;
            case PRIM_void:
                return 0;
            }
        } break;
    }
    assert(0);
    return 0;
}

char* typespec_tostring(Typespec* ty) {
    char* buf = NULL;
    bufstrexpandpush(buf, tostring(ty));
    bufpush(buf, '\0');
    return buf;
}

char* typespec_integer_get_min_value(Typespec* ty) {
    if (ty->kind == TS_TYPE) return typespec_integer_get_min_value(ty->ty);
    assert(ty->kind == TS_PRIM);
    switch (ty->prim.kind) {
        case PRIM_u8:  return "0";
        case PRIM_u16: return "0";
        case PRIM_u32: return "0";
        case PRIM_u64: return "0";
        case PRIM_i8:  return "-128";
        case PRIM_i16: return "-32768";
        case PRIM_i32: return "-2147483648";
        case PRIM_i64: return "-9223372036854775808";
        default: assert(0);
    }
    return NULL;
}

char* typespec_integer_get_max_value(Typespec* ty) {
    if (ty->kind == TS_TYPE) return typespec_integer_get_max_value(ty->ty);
    assert(ty->kind == TS_PRIM);
    switch (ty->prim.kind) {
        case PRIM_u8:  return "255";
        case PRIM_u16: return "65535";
        case PRIM_u32: return "4294967295";
        case PRIM_u64: return "18446744073709551615";
        case PRIM_i8:  return "127";
        case PRIM_i16: return "32767";
        case PRIM_i32: return "2147483647";
        case PRIM_i64: return "9223372036854775807";
        default: assert(0);
    }
    return NULL;
}
