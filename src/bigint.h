#ifndef BIGINT_H
#define BIGINT_H

#include "core.h"

#define BIGINT_DEFAULT_DIGIT_COUNT 4
#define BIGINT_DIGIT_BIT 60
#define BIGINT_MASK ((((u64)1)<<((u64)BIGINT_DIGIT_BIT))-((u64)1))
#define BIGINT_MIN_DIGIT_COUNT MAX(3, (((size_t)SIZEOF_IN_BITS(u64) + BIGINT_DIGIT_BIT) - 1) / BIGINT_DIGIT_BIT)
#define BIGINT_MAX_DIGIT_COUNT ((SIZE_MAX - 2) / BIGINT_DIGIT_BIT)

#define _bigint_malloc(size) malloc(size)
#define _bigint_calloc(nmemb, size) calloc((nmemb), (size))
#define _bigint_realloc(mem, oldsize, newsize) realloc((mem), (newsize))
#define _bigint_free(mem, size) free(mem)

#define bigint_free_digits(mem, size_in_digits) \
    _bigint_free((mem), sizeof(u64) * (size_t)(size_in_digits))

#define bigint_iszero(a)    ((a)->used == 0)
#define bigint_isneg(a)     ((a)->sign == BIGINT_SIGN_NEG) 

typedef enum {
    BIGINT_ERR_OKAY = 0,
    BIGINT_ERR_MEMORY = -1,
    BIGINT_ERR_OVERFLOW = -2,
} bigint_err;

typedef enum {
    BIGINT_SIGN_ZPOS = 0,
    BIGINT_SIGN_NEG = 1,
} bigint_sign;

typedef enum {
    BIGINT_ORD_LT = -1,
    BIGINT_ORD_EQ = 0,
    BIGINT_ORD_GT = 1,
} bigint_ord;

typedef struct {
    size_t used, alloc;
    bigint_sign sign;
    u64* d;
} bigint;

#define BIGINT_SET_FUNC(name, type) void name(bigint* a, type b)
#define BIGINT_INIT_INT_FUNC(name, type) bigint_err name(bigint* a, type b)
#define BIGINT_FITS_FUNC(name) bool name(const bigint* a)

bigint_err bigint_init(bigint* a);
bigint_err bigint_init_size(bigint* a, size_t size_in_digits);
void bigint_clear(bigint* a);
void _bigint_zero_digits(u64* d, size_t size_in_digits);
void _bigint_copy_digits(u64* d, const u64* s, size_t size_in_digits);
void bigint_clamp(bigint* a);
bigint_err bigint_grow(bigint* a, size_t size_in_digits);
bigint_err bigint_copy(const bigint* a, bigint* b);
bigint_ord bigint_cmp_mag(const bigint* a, const bigint* b);
BIGINT_SET_FUNC(bigint_set_u64, u64);
BIGINT_SET_FUNC(bigint_set_i64, i64);
BIGINT_INIT_INT_FUNC(bigint_init_u64, u64);
BIGINT_INIT_INT_FUNC(bigint_init_i64, i64);
bigint_err bigint_add(const bigint* a, const bigint* b, bigint* c);
bigint_err bigint_sub(const bigint* a, const bigint* b, bigint* c);
bigint_err bigint_neg(const bigint* a, bigint* b);
bigint_err bigint_mul(const bigint* a, const bigint* b, bigint* c);
BIGINT_FITS_FUNC(bigint_fits_u8);
BIGINT_FITS_FUNC(bigint_fits_u16);
BIGINT_FITS_FUNC(bigint_fits_u32);
BIGINT_FITS_FUNC(bigint_fits_u64);
BIGINT_FITS_FUNC(bigint_fits_i8);
BIGINT_FITS_FUNC(bigint_fits_i16);
BIGINT_FITS_FUNC(bigint_fits_i32);
BIGINT_FITS_FUNC(bigint_fits_i64);
bool bigint_fits(const bigint* a, int bytes, bigint_sign sign);
u64 bigint_get_lsd(const bigint* a);

#endif
