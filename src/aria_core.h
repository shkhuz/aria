#ifndef __ARIA_CORE_H
#define __ARIA_CORE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <stdarg.h>

///// TYPES /////
typedef unsigned int uint;
typedef unsigned char uchar;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#define null (void*)0

///// MATH /////
/* --- NOTE: do not use expressions for arguments --- */
#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define CLAMP_MAX(x, max) MIN(x, max)
#define CLAMP_MIN(x, min) MAX(x, min)

///// STRETCHY BUFFER /////
typedef struct {
	u64 len;
	u64 cap;
	char buf[];
} BufHdr;

#define _buf_hdr(b) ((BufHdr*)((char*)(b) - offsetof(BufHdr, buf)))

#define buf_len(b) ((b) ? _buf_hdr(b)->len : 0)
#define buf_cap(b) ((b) ? _buf_hdr(b)->cap : 0)
#define buf_end(b) ((b) + buf_len(b))
#define buf_sizeof(b) ((b) ? buf_len(b) * sizeof(*b) : 0)

#define buf_free(b)		   ((b) ? (free(_buf_hdr(b)), (b) = null) : 0)
#define buf_fit(b, n)	   ((n) <= buf_cap(b) ? 0 :	\
							((b) = _buf_grow((b), (n), sizeof(*(b)))))
#define buf_push(b, ...)   (buf_fit((b), 1 + buf_len(b)), \
							(b)[_buf_hdr(b)->len++] = (__VA_ARGS__))
#define buf_pop(b)		   ((b) ? _buf_shrink((b), 1) : 0)
#define buf_clear(b)	   ((b) ? _buf_hdr(b)->len = 0 : 0)
#define buf_empty(b)	   ((b) ? (buf_len(b) == 0 ? true : false) : 0)
#define buf_last(b)		   ((b) ? ((!buf_empty(b)) ? (b)[buf_len(b)-1] : 0) : 0)

#define buf_remove(b, n)   ((b) ? _buf_remove(b, n, sizeof(*(b))) : 0)

#define buf_loop(b, c) for (u64 c = 0; c < buf_len(b); ++c)

void* _buf_grow(const void* buf, u64 new_len, u64 elem_size);
void _buf_shrink(const void* buf, u64 size);
void _buf_remove(const void* buf, u64 idx, u64 elem_size);

///// STRING INTERNING /////
typedef struct {
	char* str;
	u64 len;
} StrIntern;

char* strni(char* start, char* end);
char* stri(char* str);

///// FILE I/O /////
typedef struct {
	char* fpath;
	char* contents;
	u64 len;
} File;

File* file_read(const char* fpath);
bool file_exists(const char* fpath);

///// MEMORY /////
#define alloc_with_type(name, type) type name = malloc(sizeof(type));

///// COMPILE ERROR/WARNING OUTPUT HANDLING /////
void aria_error(char* fmt, ...);
void aria_terminate();

///// COMPILE ERROR/WARNING STRINGS /////
#define ERROR_CANNOT_READ_SOURCE_FILE "error[0001]: cannot read source file '%s'"

#endif	/* __ARIA_CORE_H */
