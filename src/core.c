#include "core.h"

size_t align_to_pow2(size_t n, size_t pow2) {
    if (pow2 == 0) return n;
    return (n + (pow2-1)) & ~(pow2-1);
}

size_t get_bits_for_value(u128 n) {
    size_t count = 0;
    while (n > 0) {
            n = n >> 1;
            count++;
    }
    return count;
}

bool slice_eql_to_str(const char* slice, int slicelen, const char* str) {
    bool equal = true;
    const char* c = str;
    while (*c != '\0') {
        if (c-str == slicelen) {
            equal = false;
            break;
        }
        if (*c != slice[c-str]) {
            equal = false;
            break;
        }
        c++;
    }
    if (c-str != slicelen) equal = false;
    return equal;
}

char* format_string(const char* fmt, ...) {
    char* buf;
    va_list args;
    va_start(args, fmt);
    vasprintf(&buf, fmt, args);
    va_end(args);
    return buf;
}

void terminate_compilation() {
    exit(1);
}
