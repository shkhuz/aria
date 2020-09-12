#ifndef _BUILTIN_TYPES_H
#define _BUILTIN_TYPES_H

#include <data_type.h>

typedef struct {
    DataType* void_type;
} BuiltinTypes;

extern BuiltinTypes builtin_types;

void builtin_types_init(void);

#endif /* _BUILTIN_TYPES_H */

