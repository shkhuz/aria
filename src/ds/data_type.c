#include "data_type.h"
#include "token.h"
#include <arpch.h>

DataType data_type_new(Token* identifier, u8 pointer_count) {
    DataType data_type;
    data_type.identifier = identifier;
    data_type.pointer_count = pointer_count;
    data_type.compiler_generated = false;
    return data_type;
}

DataType* data_type_new_alloc(Token* identifier, u8 pointer_count) {
    DataType* data_type = malloc(sizeof(DataType));
    *data_type = data_type_new(identifier, pointer_count);
    return data_type;
}

DataType* data_type_from_string_int(const char* identifier, u8 pointer_count) {
    Token* token = token_from_string_type(identifier, T_IDENTIFIER);
    DataType* dt = malloc(sizeof(DataType));
    dt->identifier = token;
    dt->pointer_count = pointer_count;
    dt->compiler_generated = true;
    return dt;
}

bool is_dt_eq(DataType* a, DataType* b) {
    assert(a);
    assert(b);
    if (is_tok_eq(a->identifier, b->identifier)) {
        if (a->pointer_count == b->pointer_count) {
            return true;
        }
    }
    return false;
}

bool is_dt_eq_null(DataType* a, DataType* b) {
    bool a_present = (a ? true : false);
    bool b_present = (b ? true : false);
    if (a_present != b_present) return false;

    if (is_tok_eq(a->identifier, b->identifier)) {
        if (a->pointer_count == b->pointer_count) {
            return true;
        }
    }
    return false;
}


