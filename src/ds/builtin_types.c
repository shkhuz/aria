#include "builtin_types.h"
#include "token.h"
#include "data_type.h"

BuiltinTypes builtin_types;

void builtin_types_init(void) {
    builtin_types.e.void_type = data_type_from_string_int("void", 0);
    builtin_types.e.char_type = data_type_from_string_int("char", 0);
    builtin_types.e.u8_type = data_type_from_string_int("u8", 0);
    builtin_types.e.u16_type = data_type_from_string_int("u16", 0);
    builtin_types.e.u32_type = data_type_from_string_int("u32", 0);
    builtin_types.e.u64_type = data_type_from_string_int("u64", 0);
    builtin_types.e.i8_type = data_type_from_string_int("i8", 0);
    builtin_types.e.i16_type = data_type_from_string_int("i16", 0);
    builtin_types.e.i32_type = data_type_from_string_int("i32", 0);
    builtin_types.e.i64_type = data_type_from_string_int("i64", 0);
}

