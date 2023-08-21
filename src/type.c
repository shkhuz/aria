#include "type.h"
#include "buf.h"
#include "ast.h"

static Typespec* typespec_new(TypespecKind kind) {
    Typespec* ty = alloc_obj(Typespec);
    ty->kind = kind;
    ty->llvmtype = NULL;
    return ty;
}

Typespec* typespec_prim_new(PrimKind kind) {
    Typespec* ty = typespec_new(TS_PRIM);
    ty->prim.kind = kind;
    return ty;
}

Typespec* typespec_unsized_integer_new(bigint val) {
    Typespec* ty = typespec_new(TS_PRIM);
    ty->prim.kind = PRIM_INTEGER;
    ty->prim.integer = val;
    return ty;
}

Typespec* typespec_void_new() {
    Typespec* ty = typespec_new(TS_void);
    return ty;
}

Typespec* typespec_noreturn_new() {
    Typespec* ty = typespec_new(TS_noreturn);
    return ty;
}

Typespec* typespec_ptr_new(bool immutable, Typespec* child) {
    Typespec* ty = typespec_new(TS_PTR);
    ty->ptr.immutable = immutable;
    ty->ptr.child = child;
    return ty;
}

Typespec* typespec_multiptr_new(bool immutable, Typespec* child) {
    Typespec* ty = typespec_new(TS_MULTIPTR);
    ty->mulptr.immutable = immutable;
    ty->mulptr.child = child;
    return ty;
}

Typespec* typespec_slice_new(bool immutable, Typespec* child) {
    Typespec* ty = typespec_new(TS_SLICE);
    ty->slice.immutable = immutable;
    ty->slice.child = child;
    return ty;
}

Typespec* typespec_array_new(Typespec* size, Typespec* child) {
    Typespec* ty = typespec_new(TS_ARRAY);
    ty->array.size = size;
    ty->array.child = child;
    return ty;
}

Typespec* typespec_func_new(Typespec** params, Typespec* ret_typespec) {
    Typespec* ty = typespec_new(TS_FUNC);
    ty->func.params = params;
    ty->func.ret_typespec = ret_typespec;
    return ty;
}

Typespec* typespec_struct_new(struct AstNode* astnode) {
    Typespec* ty = typespec_new(TS_STRUCT);
    ty->agg = astnode;
    return ty;
}

Typespec* typespec_type_new(Typespec* typespec) {
    Typespec* ty = typespec_new(TS_TYPE);
    ty->ty = typespec;
    return ty;
}

Typespec* typespec_module_new(struct Srcfile* srcfile) {
    Typespec* ty = typespec_new(TS_MODULE);
    ty->mod.srcfile = srcfile;
    return ty;
}

static char* primkind_tostring(PrimKind kind) {
    char* type;
    switch (kind) {
        case PRIM_u8:   type = "u8"; break;
        case PRIM_u16:  type = "u16"; break;
        case PRIM_u32:  type = "u32"; break;
        case PRIM_u64:  type = "u64"; break;
        case PRIM_i8:   type = "i8"; break;
        case PRIM_i16:  type = "i16"; break;
        case PRIM_i32:  type = "i32"; break;
        case PRIM_i64:  type = "i64"; break;
        case PRIM_INTEGER: type = "{integer}"; break;
        case PRIM_bool: type = "bool"; break;
        default: assert(0); break;
    }
    return type;
}

static char* tostring(Typespec* ty) {
    switch (ty->kind) {
        case TS_PRIM: {
            return primkind_tostring(ty->prim.kind);
        } break;

        case TS_void:       return "void";
        case TS_noreturn:   return "noreturn";

        case TS_PTR: {
            char* buf = NULL;
            bufpush(buf, '*');
            if (ty->ptr.immutable) bufstrexpandpush(buf, "imm ");
            bufstrexpandpush(buf, tostring(ty->ptr.child));
            bufpush(buf, '\0');
            return buf;
        } break;

        case TS_MULTIPTR: {
            char* buf = NULL;
            bufstrexpandpush(buf, "[*]");
            if (ty->mulptr.immutable) bufstrexpandpush(buf, "imm ");
            bufstrexpandpush(buf, tostring(ty->mulptr.child));
            bufpush(buf, '\0');
            return buf;
        } break;

        case TS_SLICE: {
            char* buf = NULL;
            bufstrexpandpush(buf, "[]");
            if (ty->slice.immutable) bufstrexpandpush(buf, "imm ");
            bufstrexpandpush(buf, tostring(ty->slice.child));
            bufpush(buf, '\0');
            return buf;
        } break;

        case TS_ARRAY: {
            char* buf = NULL;
            bufpush(buf, '[');
            bufstrexpandpush(buf, bigint_tostring(&ty->array.size->prim.integer));
            bufpush(buf, ']');
            bufstrexpandpush(buf, tostring(ty->array.child));
            bufpush(buf, '\0');
            return buf;
        } break;

        case TS_FUNC: {
            char* buf = NULL;
            bufstrexpandpush(buf, "fn(");
            bufloop(ty->func.params, i) {
                bufstrexpandpush(buf, tostring(ty->func.params[i]));
                if (i != buflen(ty->func.params)-1) bufstrexpandpush(buf, ", ");
            }
            bufstrexpandpush(buf, ") ");
            bufstrexpandpush(buf, tostring(ty->func.ret_typespec));
            bufpush(buf, '\0');
            return buf;
        } break;

        case TS_STRUCT: {
            return ty->agg->strct.name;
        } break;

        case TS_TYPE: {
            return "{type}";
        } break;

        case TS_MODULE: {
            return "{module}";
        } break;
    }
    assert(0);
    return NULL;
}

bool typespec_is_comptime(Typespec* ty) {
    return typespec_is_unsized_integer(ty);
}

bool typespec_is_sized_integer(Typespec* ty) {
    switch (ty->kind) {
        case TS_PRIM: {
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
        } break;
        default: return false;
    }
}

bool typespec_is_unsized_integer(Typespec* ty) {
    return ty->kind == TS_PRIM && ty->prim.kind == PRIM_INTEGER;
}

bool typespec_is_integer(Typespec* ty) {
    return typespec_is_sized_integer(ty) || typespec_is_unsized_integer(ty);
}

bool typespec_is_bool(Typespec* ty) {
    return ty->kind == TS_PRIM && ty->prim.kind == PRIM_bool;
}

bool typespec_is_sized(Typespec* ty) {
    switch (ty->kind) {
        case TS_PRIM: {
            switch (ty->prim.kind) {
                case PRIM_INTEGER: return false;
                default: return true;
            }
        } break;
        case TS_FUNC: return false;
        default: return true;
    }
}

bool typespec_is_signed(Typespec* ty) {
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
        case PRIM_INTEGER:
            return ty->prim.integer.neg;
    }
    assert(0);
    return false;
}

bool typespec_is_arrptr(Typespec* ty) {
    return ty->kind == TS_PTR && ty->ptr.child->kind == TS_ARRAY;
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
                case PRIM_INTEGER: {
                    u64 d = ty->prim.integer.d[0];
                    usize bits = get_bits_for_value(d);
                    if (bits <= 8) bits = 8;
                    else if (bits <= 16) bits = 16;
                    else if (bits <= 32) bits = 32;
                    else if (bits <= 64) bits = 64;
                    return bits / 8;
                } break;
                case PRIM_bool:
                    return 1;
            }
        } break;

        case TS_void:
            return 0;

        case TS_PTR:
        case TS_MULTIPTR: {
            // TODO: make this platform specific
            return 8;
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
