#define BIGINT_DEFAULT_DIGIT_COUNT 4
#define BIGINT_DIGIT_BIT 60
#define BIGINT_MASK ((((u64)1)<<((u64)BIGINT_DIGIT_BIT))-((u64)1))
#define BIGINT_MIN_DIGIT_COUNT max((size_t)3, (((size_t)SIZEOF_IN_BITS(u64) + BIGINT_DIGIT_BIT) - 1) / BIGINT_DIGIT_BIT)
#define BIGINT_MAX_DIGIT_COUNT ((SIZE_MAX - 2) / BIGINT_DIGIT_BIT)

#define _bigint_malloc(size) malloc(size)
#define _bigint_calloc(nmemb, size) calloc((nmemb), (size))
#define _bigint_realloc(mem, oldsize, newsize) realloc((mem), (newsize))
#define _bigint_free(mem, size) free(mem)

#define bigint_free_digits(mem, size_in_digits) \
    _bigint_free((mem), sizeof(u64) * (size_t)(size_in_digits))

#define bigint_iszero(a)    ((a)->used == 0)
#define bigint_isneg(a)     ((a)->sign == BIGINT_SIGN_NEG) 

enum bigint_err {
    BIGINT_ERR_OKAY = 0,
    BIGINT_ERR_MEMORY = -1,
    BIGINT_ERR_OVERFLOW = -2,
};

enum bigint_sign {
    BIGINT_SIGN_ZPOS = 0,
    BIGINT_SIGN_NEG = 1,
};

enum bigint_ord {
    BIGINT_ORD_LT = -1,
    BIGINT_ORD_EQ = 0,
    BIGINT_ORD_GT = 1,
};

struct bigint {
    size_t used, alloc;
    bigint_sign sign;
    u64* d;
};

#define BIGINT_SET_FUNC(name, type) void name(bigint* a, type b)
#define BIGINT_INIT_INT_FUNC(name, type) bigint_err name(bigint* a, type b)
#define BIGINT_FITS_FUNC(name) bool name(const bigint* a)


bigint_err bigint_init(bigint* a) {
    a->d = (u64*)_bigint_calloc(BIGINT_DEFAULT_DIGIT_COUNT, sizeof(u64));
    if (a->d == null) return BIGINT_ERR_MEMORY;
    a->used = 0;
    a->alloc = BIGINT_DEFAULT_DIGIT_COUNT;
    a->sign = BIGINT_SIGN_ZPOS;
    return BIGINT_ERR_OKAY;
}

bigint_err bigint_init_size(bigint* a, size_t size_in_digits) {
    size_in_digits = max(BIGINT_MIN_DIGIT_COUNT, size_in_digits);
    if (size_in_digits > BIGINT_MAX_DIGIT_COUNT) {
        return BIGINT_ERR_OVERFLOW;
    }

    a->d = (u64*)_bigint_calloc(size_in_digits, sizeof(u64));
    if (a->d == null) {
        return BIGINT_ERR_MEMORY;
    }

    a->used = 0;
    a->alloc = size_in_digits;
    a->sign = BIGINT_SIGN_ZPOS;
    return BIGINT_ERR_OKAY;
}

void bigint_clear(bigint* a) {
    if (a->d != null) {
        bigint_free_digits(a->d, a->alloc);
    }
    a->d = null;
    a->alloc = a->used = 0;
    a->sign = BIGINT_SIGN_ZPOS;
}

void _bigint_zero_digits(u64* d, size_t size_in_digits) {
    zero_mem((u64*)d, size_in_digits);
}

void _bigint_copy_digits(u64* d, const u64* s, size_t size_in_digits) {
    if (size_in_digits > 0) {
        memcpy(d, s, (size_t)size_in_digits * sizeof(u64));
    }
}

void bigint_clamp(bigint* a) {
    while ((a->used > 0) && (a->d[a->used - 1] == 0u)) {
        --(a->used);
    }
    if (bigint_iszero(a)) {
        a->sign = BIGINT_SIGN_ZPOS;
    }
}

bigint_err bigint_grow(bigint* a, size_t size_in_digits) {
    if (a->alloc < size_in_digits) {
        u64* d;
        if (size_in_digits > BIGINT_MAX_DIGIT_COUNT) {
            return BIGINT_ERR_OVERFLOW;
        }

        d = (u64*)_bigint_realloc(
                a->d,
                a->alloc * sizeof(u64),
                size_in_digits * sizeof(u64));
        if (d == null) {
            // Here, a->d is still valid -- previous m/calloc call can
            // be freed without double free / use after free.
            return BIGINT_ERR_MEMORY;
        }

        a->d = d;
        _bigint_zero_digits(a->d + a->alloc, size_in_digits - a->alloc);
        a->alloc = size_in_digits;
    }
    return BIGINT_ERR_OKAY;
}

bigint_err bigint_copy(const bigint* a, bigint* b) {
    bigint_err err;
    if (a == b) {
        return BIGINT_ERR_OKAY;
    }
    if ((err = bigint_grow(b, a->used)) != BIGINT_ERR_OKAY) {
        return err;
    }

    _bigint_copy_digits(b->d, a->d, a->used);
    if (b->used > a->used) {
        _bigint_zero_digits(b->d + a->used, b->used - a->used);
    }
    b->used = a->used;
    b->sign = a->sign;
    return BIGINT_ERR_OKAY;
}

bigint_ord bigint_cmp_mag(const bigint* a, const bigint* b) {
    if (a->used != b->used) {
        return (a->used > b->used) ? BIGINT_ORD_GT : BIGINT_ORD_LT;
    }

    for (size_t i = a->used; i --> 0;) {
        if (a->d[i] != b->d[i]) {
            return (a->d[i] > b->d[i]) ? BIGINT_ORD_GT : BIGINT_ORD_LT;
        }
    }
    return BIGINT_ORD_EQ;
}

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

BIGINT_SET_UNSIGNED(bigint_set_u64, u64);
BIGINT_SET_SIGNED(bigint_set_i64, bigint_set_u64, i64, u64);
BIGINT_INIT_INT(bigint_init_u64, bigint_set_u64, u64);
BIGINT_INIT_INT(bigint_init_i64, bigint_set_i64, i64);

static bigint_err _bigint_add(const bigint* a, const bigint* b, bigint* c) {
    size_t oldused, min, max, i;
    u64 u;
    bigint_err err;
    
    if (a->used < b->used) {
        SWAP_VARS(const bigint*, a, b);
    }
    min = b->used;
    max = a->used;

    if ((err = bigint_grow(c, max + 1)) != BIGINT_ERR_OKAY) {
        return err;
    }
    oldused = c->used;
    c->used = max + 1;
    u = 0;

    for (i = 0; i < min; i++) {
        c->d[i] = a->d[i] + b->d[i] + u;
        u = c->d[i] >> (u64)BIGINT_DIGIT_BIT;
        c->d[i] &= BIGINT_MASK;
    }

    if (min != max) {
        for (; i < max; i++) {
            c->d[i] = a->d[i] + u;
            u = c->d[i] >> (u64)BIGINT_DIGIT_BIT;
            c->d[i] &= BIGINT_MASK;
        }
    }

    c->d[i] = u;
    if (oldused > c->used) {
        _bigint_zero_digits(c->d + c->used, oldused - c->used);
    }
    bigint_clamp(c);
    return BIGINT_ERR_OKAY;
}

// assumes |a| > |b|
static bigint_err _bigint_sub(const bigint* a, const bigint* b, bigint* c) {
    size_t oldused = c->used, min = b->used, max = a->used, i;
    u64 u;
    bigint_err err;

    if ((err = bigint_grow(c, max)) != BIGINT_ERR_OKAY) {
        return err;
    }
    c->used = max;
    u = 0;

    for (i = 0; i < min; i++) {
        c->d[i] = (a->d[i] - b->d[i]) - u;
        u = c->d[i] >> (SIZEOF_IN_BITS(u64) - 1u);
        c->d[i] &= BIGINT_MASK;
    }

    for (; i < max; i++) {
        c->d[i] = a->d[i] - u;
        u = c->d[i] >> (SIZEOF_IN_BITS(u64) - 1u);
        c->d[i] &= BIGINT_MASK;
    }

    if (oldused > c->used) {
        _bigint_zero_digits(c->d + c->used, oldused - c->used);
    }
    bigint_clamp(c);
    return BIGINT_ERR_OKAY;
}

bigint_err bigint_add(const bigint* a, const bigint* b, bigint* c) {
    if (a->sign == b->sign) {
        c->sign = a->sign;
        return _bigint_add(a, b, c);
    }

    if (bigint_cmp_mag(a, b) == BIGINT_ORD_LT) {
        SWAP_VARS(const bigint*, a, b);
    }

    c->sign = a->sign;
    return _bigint_sub(a, b, c);
}

bigint_err bigint_sub(const bigint* a, const bigint* b, bigint* c) {
    if (a->sign != b->sign) {
        c->sign = a->sign;
        return _bigint_add(a, b, c);
    }

    if (bigint_cmp_mag(a, b) == BIGINT_ORD_LT) {
        c->sign = (!bigint_isneg(a) ? BIGINT_SIGN_NEG : BIGINT_SIGN_ZPOS);
        SWAP_VARS(const bigint*, a, b);
    } else {
        c->sign = a->sign;
    }
    return _bigint_sub(a, b, c);
}

bigint_err bigint_neg(const bigint* a, bigint* b) {
    bigint_err err;
    if ((err = bigint_copy(a, b)) != BIGINT_ERR_OKAY) {
        return err;
    }

    b->sign = 
        ((!bigint_iszero(b) && !bigint_isneg(b)) ? 
         BIGINT_SIGN_NEG : 
         BIGINT_SIGN_ZPOS);
    return BIGINT_ERR_OKAY;
}

static bigint_err _bigint_mul(
        const bigint* a, 
        const bigint* b, 
        bigint* c, 
        size_t size_in_digits) {
    bigint t;
    bigint_err err;
    size_t pa, ix;

    if ((err = bigint_init_size(&t, size_in_digits)) != BIGINT_ERR_OKAY) {
        return err;
    }
    t.used = size_in_digits;
    pa = a->used;

    for (ix = 0; ix < pa; ix++) {
        size_t iy, pb;
        u64 u = 0;
        pb = min(b->used, size_in_digits - ix);
        for (iy = 0; iy < pb; iy++) {
            u128 r = (u128)t.d[ix + iy] + 
                     ((u128)a->d[ix] * (u128)b->d[iy]) +
                     (u128)u;
            t.d[ix + iy] = (u64)(r & (u128)BIGINT_MASK);
            u = (u64)(r >> (u128)BIGINT_DIGIT_BIT);
        }
        if ((ix + iy) < size_in_digits) {
            t.d[ix + pb] = u;
        }
    }

    bigint_clamp(&t);
    SWAP_VARS(bigint, t, *c);
    bigint_clear(&t);
    return BIGINT_ERR_OKAY;
}

bigint_err bigint_mul(const bigint* a, const bigint* b, bigint* c) {
    bigint_err err;
    /* size_t min = MIN(a->used, b->used), */
    /*        max = MAX(a->used, b->used); */
    size_t size_in_digits = a->used + b->used + 1;
    bool neg = (a->sign != b->sign);

    err = _bigint_mul(a, b, c, size_in_digits);
    c->sign = ((c->used > 0) && neg) ? BIGINT_SIGN_NEG : BIGINT_SIGN_ZPOS;
    return err;
}

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

BIGINT_FITS_UNSIGNED(bigint_fits_u8, u8);
BIGINT_FITS_UNSIGNED(bigint_fits_u16, u16);
BIGINT_FITS_UNSIGNED(bigint_fits_u32, u32);
BIGINT_FITS_UNSIGNED(bigint_fits_u64, u64);

BIGINT_FITS_SIGNED(bigint_fits_i8, INT8_MAX);
BIGINT_FITS_SIGNED(bigint_fits_i16, INT16_MAX);
BIGINT_FITS_SIGNED(bigint_fits_i32, INT32_MAX);
BIGINT_FITS_SIGNED(bigint_fits_i64, INT64_MAX);

bool bigint_fits(const bigint* a, int bytes, bigint_sign sign) {
    switch (sign) {
        case BIGINT_SIGN_ZPOS: {
            switch (bytes) {
                case 1: return bigint_fits_u8(a);
                case 2: return bigint_fits_u16(a);
                case 4: return bigint_fits_u32(a);
                case 8: return bigint_fits_u64(a);
                default: assert(0); return false;
            }
        } break;

        case BIGINT_SIGN_NEG: {
            switch (bytes) {
                case 1: return bigint_fits_i8(a);
                case 2: return bigint_fits_i16(a);
                case 4: return bigint_fits_i32(a);
                case 8: return bigint_fits_i64(a);
                default: assert(0); return false;
            }
        } break;
    }
    assert(0);
    return false;
}

u64 bigint_get_lsd(const bigint* a) {
    u128 n = ((a->d[1] << BIGINT_DIGIT_BIT) | a->d[0]);
    if (get_bits_for_value(n) <= 64) {
        return (u64)n;
    }
    assert(0);
    return 0;
}

