#ifndef TYPE_H
#define TYPE_H

struct AstNode;

typedef enum {
    TS_PRIM,
    TS_STRUCT,
    TS_UNION,
    TS_ENUM,
} TypespecKind;

typedef struct {
    TypespecKind kind;
    union {
        struct AstNode* agg;
    };
} Typespec;

#endif
