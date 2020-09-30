#ifndef _DATA_TYPE_H
#define _DATA_TYPE_H

#include "token.h"
#include <arpch.h>

typedef struct {
    Token* identifier;
    u8 pointer_count;
    bool compiler_generated;
} DataType;

DataType data_type_new(Token* identifier, u8 pointer_count);
DataType* data_type_new_alloc(Token* identifier, u8 pointer_count);
DataType* data_type_from_string_int(const char* identifier, u8 pointer_count);
bool is_dt_eq(DataType* a, DataType* b);
bool is_dt_integer(DataType* dt);

#endif /* _DATA_TYPE_H */
