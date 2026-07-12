#ifndef CORE_H
#define CORE_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>

#ifdef __linux__
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define PLATFORM_AMD64
#elif defined(__aarch64__) || defined(_M_ARM64)
#define PLATFORM_AARCH64
#else
#define PLATFORM_UNKNOWN
#endif

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

typedef size_t usize;
typedef ssize_t isize;

#define U8_BITS  8
#define U16_BITS 16
#define U32_BITS 32
#define U64_BITS 64

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP_MIN(x, min) (MAX(x, min))
#define CLAMP_MAX(x, max) (MIN(x, max))

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define SIZEOF_IN_BITS(x) ((size_t)8 * sizeof(x))
#define SWAP_VARS(t, a, b) do { t _c = a; a = b; b = _c; } while (0)
#define COMBINE1(X, Y) X##Y
#define COMBINE(X,Y) COMBINE1(X,Y)
#define STRINGIFY1(X) #X
#define STRINGIFY(X) STRINGIFY1(X)
#define ENUM_GEN(ENUM) ENUM,
#define STRING_GEN(STRING) #STRING,
#define ALLOC_OBJ(type) ((type*)xmalloc(sizeof(type)))

usize align_to_pow2(size_t n, size_t pow2);
usize u64_bitlength(u64 n);
usize get_bits_for_value(u128 n);
int char_to_digit(char c);
bool is_octal_digit(char c);
u64 maxinteger_unsigned(int bytes);
u64 maxinteger_signed(int bytes);
// Checks if a slice is equal in contents to a string, NOT VICE VERSA.
// (ie. compare string and slice but till the length of the string)
// For example:
// slice:  "str", 3
// string: "struct\0"
// This function will return false.
// Using strncmp, this comparison will return 0 (equal) even if the
// contents are different, because the `len` parameter will clamp the string
// from "struct" to "str", hence returning 0. Though if we strlen before
// calling strncmp, it would work, but is inefficient.
bool slice_eql_to_str(const char* slice, int slicelen, const char* str);
char* format_string(const char* fmt, ...);
u64 hash_string(const char* str);

// =============================================================================
// BUFFER
// =============================================================================

typedef struct {
    usize cap;
    usize len;
    char data[];
} bufhdr;

#define _bufhdr(b) ((bufhdr*)((char*)(b) - offsetof(bufhdr, data)))
#define bufend(b) ((b) + buflen(b))
#define buflast(b) (buflen((b)) == 0 ? (NULL) : (bufend((b))-1))
#define buflastidx(b) (buflen(b)-1)

#define buffit(b, n) (bufcap(b) >= n ? 0 : \
    ((b) = _bufgrow((b), (n), sizeof(*(b)))))

#define bufpush(b, ...) (buffit((b), 1 + buflen((b))), \
    ((b)[_bufhdr((b))->len++] = __VA_ARGS__))

// This macro takes a char buffer and a string, and pushes the string content
// into the buffer, not the pointer to the string.
#define bufstrexpandpush(b, e) {usize COMBINE(__tmpsize, __LINE__) = strlen((e)); \
    (buffit((b), (COMBINE(__tmpsize, __LINE__)) + buflen((b))), \
    (memcpy(&((b)[_bufhdr((b))->len]), (e), (COMBINE(__tmpsize, __LINE__)))), \
    (_bufhdr((b))->len += (COMBINE(__tmpsize, __LINE__))));}

#define bufpop(b) (buflen(b) > 0 ? (_bufhdr((b))->len--) : 0)

#define buffree(b) ((b) ? (xfree(_bufhdr(b)), b=NULL) : 0)

#define bufloop(b, c) for (usize c = 0; c < buflen(b); c++)
#define bufrevloop(b, c) for (usize c = buflen(b); c-- > 0 ;)

#define bufinsert(b, i, ...) (buffit((b), 1 + buflen((b))), \
    memmove((b+i+1), (b+i), (_bufhdr((b))->len-i) * sizeof(*b)), \
    ((b)[i] = __VA_ARGS__), \
    _bufhdr((b))->len++)
#define bufclear(b) ((b) ? _bufhdr((b))->len = 0 : 0)

usize buflen(const void* buf);
usize bufcap(const void* buf);
void* _bufgrow(const void* buf, usize new_len, usize elem_size);

// =============================================================================
// FILE IO
// =============================================================================

typedef struct {
    const char* path;
    const char* abs_path;
    const char* contents;
    usize len;
} File;

typedef enum {
    FILEIO_FAILURE,
    FILEIO_SUCCESS,
    FILEIO_DIRECTORY,
} FileOpResult;

typedef struct {
    File handle;
    FileOpResult status;
} FileOrError;

bool file_exists(const char* path);
int is_dir(const char* path);
FileOrError read_file(const char* path);
FileOpResult write_bin_file(const char* path, const char* contents, u64 bytes);
const char* file_get_line_ptr(const File* handle, usize line);

// =============================================================================
// CLI COLORS
// =============================================================================

#define EC_8BITCOLOR(colorstr, boldstr) \
    "\x1B[" boldstr ";38;5;" colorstr "m"

extern char* g_green_color;
extern char* g_bold_green_color;
extern char* g_red_color;
extern char* g_bold_red_color;
extern char* g_error_color;
extern char* g_warning_color;
extern char* g_note_color;
extern char* g_bold_color;
extern char* g_bold_grey_color;
extern char* g_grey_color;
extern char* g_reset_color;
extern char* g_bold_cornflower_blue_color;

// =============================================================================
// BIGINT
// =============================================================================

typedef struct {
    u64* d;
    bool neg;
} bigint;

extern bigint BIGINT_ZERO;

bigint bigint_new();
bigint bigint_new_u64(u64 num);
void bigint_clear(bigint* a);
void bigint_normalize(bigint* a);
void bigint_set_u64(bigint* a, u64 num);
usize bigint_bitlength(const bigint* a);
void bigint_copy(bigint* dest, const bigint* src);
void bigint_free(bigint* a);
int bigint_cmp_abs(const bigint* a, const bigint* b);
int bigint_cmp(const bigint* a, const bigint* b);
void bigint_neg(bigint* a);
void bigint_add_unsigned(bigint* a, const bigint* b);
void bigint_sub_unsigned(bigint* a, const bigint* b);
void bigint_add_signed(bigint* a, bool aneg, const bigint* b, bool bneg);
void bigint_add(bigint* a, const bigint* b);
void bigint_sub(bigint* a, const bigint* b);
void bigint_shl(bigint* a);
void bigint_shln(bigint* a, usize n);
void bigint_shr(bigint* a);
void bigint_shrn(bigint* a, usize n);
void bigint_set_bit(bigint* a, usize bit, bool set);
void bigint_mul(bigint* a, const bigint* b);
void bigint_div_mod(const bigint* num, const bigint* den, bigint* quo, bigint* rem);
bool bigint_fits(const bigint* a, int bytes, bool signd);
char* bigint_tostring(const bigint* a);
void test_bigint();

// =============================================================================
// MEM_STATS
// =============================================================================

void* tracked_malloc(
    usize size, 
    const char* file, 
    int line
);
void tracked_free(void* user_ptr);
void* tracked_realloc(
    void* user_ptr, 
    usize new_size, 
    const char* file, 
    int line
);
void* tracked_calloc(
    usize num, 
    usize size, 
    const char* file, 
    int line
);
void print_mem_stats();

#define xmalloc(s)      tracked_malloc(s, __FILE__, __LINE__)
#define xcalloc(n, s)   tracked_calloc(n, s, __FILE__, __LINE__)
#define xrealloc(p, s)  tracked_realloc(p, s, __FILE__, __LINE__)
#define xfree(p)        tracked_free(p)

void init_core();

#endif
