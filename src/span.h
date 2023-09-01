#ifndef SPAN_H
#define SPAN_H

#include "core.h"

typedef struct {
    struct Srcfile* srcfile;
    usize start, end;
} Span;

typedef struct {
    Span span;
    bool exists;
} OptionalSpan;

Span span_new(struct Srcfile* srcfile, usize start, usize end);
Span span_from_two(Span start, Span end);

OptionalSpan span_some(Span span);
OptionalSpan span_none();
char* span_tostring(Span span);

#endif
