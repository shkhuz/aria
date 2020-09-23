#ifndef _BUILTIN_TYPES_H
#define _BUILTIN_TYPES_H

#include "data_type.h"

#define BUILTIN_TYPES_LEN \
    (sizeof(struct BuiltinTypesStruct) / sizeof(DataType*))

struct BuiltinTypesStruct {
    DataType* void_type;
    DataType* char_type;
    DataType* u8_type;
    DataType* u16_type;
    DataType* u32_type;
    DataType* u64_type;
    DataType* i8_type;
    DataType* i16_type;
    DataType* i32_type;
    DataType* i64_type;
};

typedef union {
    struct BuiltinTypesStruct e;
    DataType* l[BUILTIN_TYPES_LEN];
} BuiltinTypes;

extern BuiltinTypes builtin_types;

void builtin_types_init(void);

#endif /* _BUILTIN_TYPES_H */


