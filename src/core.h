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

#ifdef __linux__
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
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
typedef ssize_t ssize;

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

size_t align_to_pow2(size_t n, size_t pow2);
size_t get_bits_for_value(u128 n);
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
void terminate_compilation();

extern char* g_exec_path;

#endif
