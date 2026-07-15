#ifndef SRCFILE_H
#define SRCFILE_H

#include "types.h"
#include "core.h"

struct Srcfile {
    File handle;
    Token* tokens;
    Node* nodes;
    int* nextra;
};

typedef struct {
    int start, end;
} Span;

Span span_new(int start, int end);
Span span_from_two(Span start, Span end);
Span span_only_firstchar(Span span);
char* span_tostring(Span span, Srcfile* srcfile);

#endif
