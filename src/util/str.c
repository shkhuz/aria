#include "str.h"
#include "buf.h"

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
    memcpy(res, a, al);
    memcpy(res + al, b, bl);
    res[al + bl] = '\0';
    return res;
}

char* change_ext_for_path(const char* path, const char* ext) {
    FindCharResult dot = find_last_of(path, '.');
    char* modified_path = null;
    char* basename = (char*)path;
    if (dot.found) {
        basename = substr(path, 0, dot.pos);
    }

    for (u64 c = 0; c < strlen(basename); c++) {
        buf_push(modified_path, basename[c]);
    }
    buf_push(modified_path, '.');
    for (u64 c = 0; c < strlen(ext); c++) {
        buf_push(modified_path, ext[c]);
    }
    buf_push(modified_path, 0);
    return modified_path;
}

char* not_dir(const char* path) {
    FindCharResult slash = find_last_of(path, '/');
    if (!slash.found) return (char*)path;

    return substr(path, slash.pos + 1, strlen(path));
}

