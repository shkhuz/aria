#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <limits.h>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <fmt/core.h>

#ifdef __linux__
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

typedef unsigned int uint;
typedef unsigned char uchar;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef u64 u128 __attribute__((mode(TI)));

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef i64 i128 __attribute__((mode(TI)));

#define null nullptr

#define ANSI_FBOLD    "\x1B[1m"
#define ANSI_FRED     "\x1B[1;31m"
#define ANSI_FGREEN   "\x1B[1;32m"
#define ANSI_FYELLOW  "\x1B[33m"
#define ANSI_FBLUE    "\x1B[34m"
#define ANSI_FMAGENTA "\x1B[1;35m"
#define ANSI_FCYAN    "\x1B[1;36m"
#define ANSI_FCYANN    "\x1B[0;36m"
#define ANSI_RESET    "\x1B[0m"

#define ANSI_FERROR_COLOR ANSI_FRED
#define ANSI_FWARNING_COLOR ANSI_FMAGENTA
#define ANSI_FNOTE_COLOR ANSI_FCYAN

template <typename T>
T min(T x, T y) {
    return (x <= y ? x : y);
}

template <typename T>
T max(T x, T y) {
    return (x >= y ? x : y);
}

template <typename T>
T clamp_min(T x, T max) {
    return min(x, max);
}

template <typename T>
T clamp_max(T x, T min) {
    return max(x, min);
}

template <typename T>
void* zero_mem(T* mem, size_t count) {
    return memset(mem, 0, count*sizeof(*mem));
}

#define STCK_ARR_LEN(arr) sizeof(arr) / sizeof(arr[0])
#define SIZEOF_IN_BITS(x) ((size_t)8 * sizeof(x))
#define ALLOC_WITH_TYPE(name, type) type* name = new type
#define ALLOC_WITH_TYPE_ASSIGN(name, type) name = new type
#define SWAP_VARS(t, a, b) do { t _c = a; a = b; b = _c; } while (0)
#define COMBINE1(X, Y) X##Y
#define COMBINE(X,Y) COMBINE1(X,Y)

char* g_error_color = ANSI_FERROR_COLOR;
char* g_warning_color = ANSI_FWARNING_COLOR;
char* g_note_color = ANSI_FNOTE_COLOR;
char* g_bold_color = ANSI_FBOLD;
char* g_reset_color = ANSI_RESET;
char* g_fcyan_color = ANSI_FCYAN;
char* g_fcyann_color = ANSI_FCYANN;

void init_core() {
    if (!isatty(2)) {
        g_error_color = "";
        g_warning_color = "";
        g_note_color = "";
        g_bold_color = "";
        g_reset_color = "";
        g_fcyan_color = "";
        g_fcyann_color = "";
    }
}

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

int char_to_digit(char c) {
    return c - 48;
}

static std::vector<size_t> child_ids;

size_t id_register_parent() {
    size_t parent_id = child_ids.size();
    child_ids.push_back(0);
    return parent_id;
}

size_t id_next(size_t parent_id) {
    if (parent_id < child_ids.size()) {
        return child_ids[parent_id]++;
    }
    else {
        assert(0); // invalid parent id
    }
}