#include "core.h"

usize align_to_pow2(size_t n, size_t pow2) {
    if (pow2 == 0) return n;
    return (n + (pow2-1)) & ~(pow2-1);
}

usize u64_bitlength(u64 n) {
    for (int i = U64_BITS-1; i >= 0; i--) {
        if ((n >> i) & 1) return i+1;
    }
    return 0;
}

usize get_bits_for_value(u128 n) {
    size_t count = 0;
    while (n > 0) {
            n = n >> 1;
            count++;
    }
    return count;
}

u64 maxinteger_unsigned(int bytes) {
    switch (bytes) {
        case 1: return UINT8_MAX;
        case 2: return UINT16_MAX;
        case 4: return UINT32_MAX;
        case 8: return UINT64_MAX;
        default: assert(0 && "maxinteger: Invalid byte count"); break;
    }
}

u64 maxinteger_signed(int bytes) {
    switch (bytes) {
        case 1: return INT8_MAX;
        case 2: return INT16_MAX;
        case 4: return INT32_MAX;
        case 8: return INT64_MAX;
        default: assert(0 && "maxinteger: Invalid byte count"); break;
    }
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

u64 hash_string(const char* str) {
    u64 hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}

void terminate_compilation() {
    exit(1);
}
