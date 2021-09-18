#include "stri.h"
#include "buf.h"

static StrIntern* g_interns;

char* strni(char* start, char* end) {
    size_t len = end - start;
    buf_loop(g_interns, i) {
        if (g_interns[i].len == len && 
            strncmp(g_interns[i].str, start, len) == false) {
            return g_interns[i].str;
        }
    }

    char* str = (char*)aria_malloc(len + 1);
    memcpy(str, start, len);
    str[len] = 0;

    StrIntern new_stri;
    new_stri.str = str;
    new_stri.len = len;
    buf_push(g_interns, new_stri);
    return str;
}

char* stri(char* str) {
    return strni(str, str + strlen(str));
}
