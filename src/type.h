#ifndef TYPE_H
#define TYPE_H

#include "core.h"
#include "bigint.h"

#include <llvm-c/Core.h>

struct AstNode;
struct Srcfile;

typedef enum {
    TS_PRIM,

    TS_void,
    TS_noreturn,

    TS_PTR,
    TS_MULTIPTR,
    TS_SLICE,
    TS_ARRAY,

    TS_STRUCT,
    TS_UNION,
    TS_ENUM,

    TS_TYPE,
    TS_MODULE,
    TS_FUNC,
} TypespecKind;

typedef enum {
    PRIM_u8,
    PRIM_u16,
    PRIM_u32,
    PRIM_u64,
    PRIM_i8,
    PRIM_i16,
    PRIM_i32,
    PRIM_i64,
    PRIM_INTEGER,
    PRIM_bool,
} PrimKind;

typedef struct Typespec {
    TypespecKind kind;

    LLVMTypeRef llvmtype;

    union {
        struct {
            PrimKind kind;
            bigint integer;
        } prim;

        struct {
            bool immutable;
            struct Typespec* child;
        } ptr;

        struct {
            bool immutable;
            struct Typespec* child;
        } mulptr;

        struct {
            bool immutable;
            struct Typespec* child;
        } slice;

        struct {
            struct Typespec* size;
            struct Typespec* child;
        } array;

        struct {
            struct Typespec** params;
            struct Typespec* ret_typespec;
        } func;

        struct {
            struct AstNode* ref;
            bool pass_by_ref;
        } agg;

        struct Typespec* ty;

        struct {
            struct Srcfile* srcfile;
        } mod;
    };
} Typespec;

Typespec* typespec_prim_new(PrimKind kind);
Typespec* typespec_unsized_integer_new(bigint val);
Typespec* typespec_void_new();
Typespec* typespec_noreturn_new();
Typespec* typespec_ptr_new(bool immutable, Typespec* child);
Typespec* typespec_multiptr_new(bool immutable, Typespec* child);
Typespec* typespec_slice_new(bool immutable, Typespec* child);
Typespec* typespec_array_new(Typespec* size, Typespec* child);
Typespec* typespec_func_new(Typespec** params, Typespec* ret_typespec);
Typespec* typespec_struct_new(struct AstNode* astnode);
Typespec* typespec_type_new(Typespec* typespec);
Typespec* typespec_module_new(struct Srcfile* srcfile);

bool typespec_is_comptime(Typespec* ty);
bool typespec_is_sized_integer(Typespec* ty);
bool typespec_is_unsized_integer(Typespec* ty);
bool typespec_is_integer(Typespec* ty);
bool typespec_is_bool(Typespec* ty);
bool typespec_is_sized(Typespec* ty);
bool typespec_is_signed(Typespec* ty);
bool typespec_is_arrptr(Typespec* ty);
// Only use after semantic analysis.
bool typespec_is_pass_by_ref(Typespec* ty);
u32 typespec_get_bytes(Typespec* ty);
char* typespec_tostring(Typespec* ty);

char* typespec_integer_get_min_value(Typespec* ty);
char* typespec_integer_get_max_value(Typespec* ty);

#endif
