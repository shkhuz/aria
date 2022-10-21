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
    
void terminate_compilation() {
    exit(1);
}
