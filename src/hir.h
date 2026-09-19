#ifndef HIR_H
#define HIR_H

struct HirInst {
    HirInstKind kind;
    Span span;
    int lhs, rhs;
};

#endif
