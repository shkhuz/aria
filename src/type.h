#ifndef TYPE_H
#define TYPE_H

struct AstNode;

typedef enum {
    TS_PRIM,
    TS_STRUCT,
    TS_UNION,
    TS_ENUM,

    TS_TYPE,
} TypespecKind;

typedef struct Typespec {
    TypespecKind kind;
    union {
        struct AstNode* agg;
        struct Typespec* ty;
    };
} Typespec;

#endif
