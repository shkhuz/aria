#include "core.h"
#include "aria.h"

void* aria_malloc(size_t bytes) {
    return malloc(bytes);
}

void* aria_realloc(void* buf, size_t bytes) {
    return realloc(buf, bytes);
}

void aria_free(void* buf) {
    return free(buf);
}

size_t round_to_next_multiple(size_t n, size_t multiple) {
	if (multiple == 0) return n;

	size_t remainder = n % multiple;
	if (remainder == 0) return n;

	return n + multiple - remainder;
}

size_t get_bits_for_value(u128 n) {
	size_t count = 0;
	while (n > 0) {
		n = n >> 1;
		count++;
	}
	return count;
}

int char_to_digit(char c) {
    return c - 48;
}
