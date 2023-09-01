#include "span.h"
#include "compile.h"

Span span_new(struct Srcfile* srcfile, usize start, usize end) {
    return (Span){
        srcfile,
        start,
        end
    };
}

Span span_from_two(Span start, Span end) {
    if (start.srcfile != end.srcfile) assert(0);
    return span_new(start.srcfile, start.start, end.end);
}

OptionalSpan span_some(Span span) {
    return (OptionalSpan){
        span,
        true
    };
}

OptionalSpan span_none() {
    return (OptionalSpan){
        (Span){ 0 },
        false
    };
}

char* span_tostring(Span span) {
    usize len = span.end - span.start;
    char* buf = malloc(len + 1);
    memcpy(buf, &span.srcfile->handle.contents[span.start], len);
    buf[len] = '\0';
    return buf;
}
