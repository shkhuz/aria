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

#define STCK_ARR_LEN(arr) sizeof(arr) / sizeof(arr[0])
#define SIZEOF_IN_BITS(x) ((size_t)8 * sizeof(x))
#define SWAP_VARS(t, a, b) do { t _c = a; a = b; b = _c; } while (0)
#define COMBINE1(X, Y) X##Y
#define COMBINE(X,Y) COMBINE1(X,Y)
#define STRINGIFY1(X) #X
#define STRINGIFY(X) STRINGIFY1(X)

#define alloc_obj(type) (type*)malloc(sizeof(type))

usize align_to_pow2(size_t n, size_t pow2);
usize u64_bitlength(u64 n);
usize get_bits_for_value(u128 n);
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

extern char g_exec_path[PATH_MAX+1];
extern char* g_lib_path;

#endif
