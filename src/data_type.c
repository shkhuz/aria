#include <data_type.h>
#include <arpch.h>

DataType data_type_new(Token* identifier, u8 pointer_count) {
    DataType data_type;
    data_type.identifier = identifier;
    data_type.pointer_count = pointer_count;
    return data_type;
}

DataType* data_type_new_alloc(Token* identifier, u8 pointer_count) {
    DataType* data_type = malloc(sizeof(DataType));
    *data_type = data_type_new(identifier, pointer_count);
    return data_type;
}

bool is_dt_eq(DataType* a, DataType* b) {
    if (is_tok_eq(a->identifier, b->identifier)) {
        if (a->pointer_count == b->pointer_count) {
            return true;
        }
    }
    return false;
}
