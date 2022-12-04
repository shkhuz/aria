#ifndef BIGINT_H
#define BIGINT_H

#include "core.h"

#define BIGINT_DEFAULT_DIGIT_COUNT 4
#define BIGINT_DIGIT_BIT 60
#define BIGINT_MASK ((((u64)1)<<((u64)BIGINT_DIGIT_BIT))-((u64)1))
#define BIGINT_MIN_DIGIT_COUNT MAX((size_t)3, (((size_t)SIZEOF_IN_BITS(u64) + BIGINT_DIGIT_BIT) - 1) / BIGINT_DIGIT_BIT)
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

BIGINT_SET_FUNC(bigint_set_u64, u64);
BIGINT_SET_FUNC(bigint_set_i64, i64);
BIGINT_INIT_INT_FUNC(bigint_init_u64, u64);
BIGINT_INIT_INT_FUNC(bigint_init_i64, i64);

BIGINT_FITS_FUNC(bigint_fits_u8);
BIGINT_FITS_FUNC(bigint_fits_u16);
BIGINT_FITS_FUNC(bigint_fits_u32);
BIGINT_FITS_FUNC(bigint_fits_u64);

BIGINT_FITS_FUNC(bigint_fits_i8);
BIGINT_FITS_FUNC(bigint_fits_i16);
BIGINT_FITS_FUNC(bigint_fits_i32);
BIGINT_FITS_FUNC(bigint_fits_i64);

#define BIGINT_SET_UNSIGNED(name, type)                                         \
    BIGINT_SET_FUNC(name, type) {                                               \
        size_t i = 0;                                                           \
        size_t b_size_in_bits = SIZEOF_IN_BITS(b);                              \
        while (b != 0) {                                                        \
            a->d[i++] = ((u64)b & BIGINT_MASK);                                 \
            if (b_size_in_bits <= BIGINT_DIGIT_BIT) { break; }                  \
            b >>= ((b_size_in_bits <= BIGINT_DIGIT_BIT) ? 0 : BIGINT_DIGIT_BIT);\
        }                                                                       \
        a->used = i;                                                            \
        a->sign = BIGINT_SIGN_ZPOS;                                             \
        _bigint_zero_digits(a->d + a->used, a->alloc - a->used);                \
    }                                                                           \

#define BIGINT_SET_SIGNED(name, uname_func, type, utype)                        \
    BIGINT_SET_FUNC(name, type) {                                               \
        uname_func(a, (b < 0) ? -(utype)b : (utype)b);                          \
        if (b < 0) { a->sign = BIGINT_SIGN_NEG; }                               \
    }                                                                           \

#define BIGINT_INIT_INT(name, set_func, type)                                   \
    BIGINT_INIT_INT_FUNC(name, type) {                                          \
        bigint_err err;                                                         \
        if ((err = bigint_init(a)) != BIGINT_ERR_OKAY) {                        \
            return err;                                                         \
        }                                                                       \
        set_func(a, b);                                                         \
        return BIGINT_ERR_OKAY;                                                 \
    }                                                                           \

#define BIGINT_FITS_UNSIGNED(name, type) \
    BIGINT_FITS_FUNC(name) { \
        if (a->used > 2 || (a->used == 2 && (a->d[1] & 0xfffffffffffffff0) != 0) || bigint_isneg(a)) { \
            return false; \
        } \
        u128 n = ((a->d[1] << BIGINT_DIGIT_BIT) | a->d[0]); \
        if (get_bits_for_value(n) <= SIZEOF_IN_BITS(type)) { \
            return true; \
        } \
        return false; \
    }

#define BIGINT_FITS_SIGNED(name, max) \
    BIGINT_FITS_FUNC(name) { \
        if (a->used > 2 || (a->used == 2 && (a->d[1] & 0xfffffffffffffff0) != 0)) { \
            return false; \
        } \
        u128 n = ((a->d[1] << BIGINT_DIGIT_BIT) | a->d[0]); \
        if (bigint_isneg(a)) { \
            if (n > (((u128)(max))+1)) { \
                return false; \
            } \
        } else { \
            if (n > (u128)(max)) { \
                return false; \
            } \
        } \
        return true; \
    }

bigint_err bigint_init(bigint* a);
bigint_err bigint_init_size(bigint* a, size_t size_in_digits);
void bigint_free(bigint* a);
void _bigint_zero_digits(u64* d, size_t size_in_digits);
void bigint_clear(bigint* a);
void _bigint_copy_digits(u64* d, const u64* s, size_t size_in_digits);
void bigint_clamp(bigint* a);
bigint_err bigint_grow(bigint* a, size_t size_in_digits);
bigint_err bigint_copy(const bigint* a, bigint* b);
bigint_ord bigint_cmp_mag(const bigint* a, const bigint* b);
bigint_err bigint_add(const bigint* a, const bigint* b, bigint* c);
bigint_err bigint_sub(const bigint* a, const bigint* b, bigint* c);
bigint_err bigint_neg(const bigint* a, bigint* b);
bigint_err bigint_mul(const bigint* a, const bigint* b, bigint* c);
bool bigint_fits(const bigint* a, int bytes, bool signd);
u64 bigint_get_lsd(const bigint* a);

#endif
