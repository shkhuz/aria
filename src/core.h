#ifndef CORE_H
#define CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef size_t usize;
typedef ssize_t ssize;

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP_MIN(x, min) (MAX(x, min))
#define CLAMP_MAX(x, max) (MIN(x, max))

#endif
