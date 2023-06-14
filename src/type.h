#ifndef TYPE_H
#define TYPE_H

#include "core.h"
#include "bigint.h"

struct AstNode;
struct Srcfile;

typedef enum {
    TS_PRIM,
    TS_PTR,
    TS_MULTIPTR,
    TS_FUNC,

    TS_STRUCT,
    TS_UNION,
    TS_ENUM,

    TS_TYPE,
    TS_MODULE,
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
    PRIM_void,
    PRIM_comptime_integer,
} PrimKind;

typedef struct Typespec {
    TypespecKind kind;
    union {
        struct {
            PrimKind kind;
            bigint comptime_integer;
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
            struct Typespec** params;
            struct Typespec* ret_typespec;
        } func;

        struct AstNode* agg;

        struct Typespec* ty;

        struct {
            struct Srcfile* srcfile;
        } mod;
    };
} Typespec;

Typespec* typespec_prim_new(PrimKind kind);
Typespec* typespec_comptime_integer_new(bigint val);
Typespec* typespec_ptr_new(bool immutable, Typespec* child);
Typespec* typespec_multiptr_new(bool immutable, Typespec* child);
Typespec* typespec_func_new(Typespec** params, Typespec* ret_typespec);
Typespec* typespec_struct_new(struct AstNode* astnode);
Typespec* typespec_type_new(Typespec* typespec);
Typespec* typespec_module_new(struct Srcfile* srcfile);

bool typespec_is_integer(Typespec* ty);
bool typespec_is_comptime_integer(Typespec* ty);
bool typespec_is_signed(Typespec* ty);
u32 typespec_get_bytes(Typespec* ty);
char* typespec_tostring(Typespec* ty);

char* typespec_integer_get_min_value(Typespec* ty);
char* typespec_integer_get_max_value(Typespec* ty);

#endif