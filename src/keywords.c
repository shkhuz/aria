#include <arpch.h>

const char** keywords;

void keywords_init(void) {
    keywords = null;
    buf_push(keywords, str_intern("fn"));
    buf_push(keywords, str_intern("pub"));
}
