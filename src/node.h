#ifndef NODE_H 
#define NODE_H

#include "types.h"
#include "core.h"
#include "srcfile.h"

typedef enum {
    #define WRAP(KIND) KIND,
    #include "nodekind.def"
    #undef WRAP
} NodeKind;

extern const char* nodekind_strs[];

struct Node {
    NodeKind kind;
    Span span;
    int lhs, rhs;
};

#endif
