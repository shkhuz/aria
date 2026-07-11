#include "srcfile.h"
#include "compile.h"

Span span_new(int start, int end) {
    return (Span){
        start,
        end
    };
}

Span span_from_two(Span start, Span end) {
    return span_new(start.start, end.end);
}

Span span_only_firstchar(Span span) {
    return span_new(span.start, span.start+1);
}

// TODO: maybe remove these two?
char TOSTRING_BUF[1024];

char* span_tostring(Span span, Srcfile* srcfile) {
    int len = span.end - span.start;
    char* buf = TOSTRING_BUF;
    memcpy(buf, &srcfile->handle.contents[span.start], len);
    buf[len] = '\0';
    return buf;
}
