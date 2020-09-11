#include <str.h>

FindCharResult find_last_of(const char* str, char c) {
    assert(str);
    u64 len = strlen(str);
    FindCharResult res;
    res.found = false;
    res.pos = len;

    u64 last_found_index = len;
    for (u64 i = 0; i < len; i++) {
        if (str[i] == c) last_found_index = i;
    }

    if (last_found_index != len) {
        res.found = true;
        res.pos = last_found_index;
        return res;
    }
    return res;
}

char* substr(const char* str, u64 start, u64 end) {
    u64 len = strlen(str);
    assert(str);
    assert(end <= len);
    assert(start < end);

    char* res = malloc(end - start + 1);
    strncpy(res, str + start, end - start);
    res[end - start] = '\0';
    return res;
}

char* concat(const char* a, const char* b) {
    u64 al = strlen(a);
    u64 bl = strlen(b);

    char* res = malloc(al + bl + 1);
    strncpy(res, a, al);
    strncpy(res + al, b, bl);
    res[al + bl] = '\0';
    return res;
}
