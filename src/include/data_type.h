#ifndef _DATA_TYPE_H
#define _DATA_TYPE_H

#include <token.h>
#include <arpch.h>

typedef struct {
    Token* identifier;
    u8 pointer_count;
} DataType;

DataType data_type_new(Token* identifier, u8 pointer_count);
DataType* data_type_new_alloc(Token* identifier, u8 pointer_count);

#endif /* _DATA_TYPE_H */
