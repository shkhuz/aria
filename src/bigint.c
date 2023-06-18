#include "bigint.h"
#include "buf.h"

#define WORD_HALF_MASK 0x00000000ffffffff

bigint BIGINT_ZERO;

void init_bigint() {
    BIGINT_ZERO = bigint_new_u64(0);
}

bigint bigint_new() {
    bigint b;
    b.d = NULL;
    b.neg = false;
    return b;
}

bigint bigint_new_u64(u64 num) {
    bigint b = bigint_new();
    bufpush(b.d, num);
    bigint_normalize(&b);
    return b;
}

void bigint_clear(bigint* a) {
    bufclear(a->d);
    a->neg = false;
}

void bigint_normalize(bigint* a) {
    while (buflen(a->d) > 0 && *buflast(a->d) == 0) {
        bufpop(a->d);
    }
    if (buflen(a->d) == 0) a->neg = false;
}

void bigint_set_u64(bigint* a, u64 num) {
    bigint_clear(a);
    bufpush(a->d, num);
    bigint_normalize(a);
}

usize bigint_bitlength(const bigint* a) {
    if (buflen(a->d) == 0) return 0;
    usize msw_idx = buflen(a->d)-1;
    return u64_bitlength(a->d[msw_idx]) + msw_idx*U64_BITS;
}

void bigint_copy(bigint* dest, const bigint* src) {
    bigint_clear(dest);
    bufloop(src->d, i) {
        bufpush(dest->d, src->d[i]);
    }
    dest->neg = src->neg;
    bigint_normalize(dest);
}

void bigint_free(bigint* a) {
    buffree(a->d);
}

int bigint_cmp_abs(const bigint* a, const bigint* b) {
    usize na = buflen(a->d);
    usize nb = buflen(b->d);
    if (na == nb) {
        for (usize i = na; i --> 0;) {
            if (a->d[i] != b->d[i]) {
                return a->d[i] > b->d[i] ? 1 : -1;
            }
        }
        return 0;
    } else {
        return na > nb ? 1 : -1;
    }
}

int bigint_cmp(const bigint* a, const bigint* b) {
    if (buflen(a->d) == 0 && buflen(b->d) == 0) return 0;
    else if (!a->neg && !b->neg) return  bigint_cmp_abs(a, b);
    else if ( a->neg &&  b->neg) return -bigint_cmp_abs(a, b);
    else return !a->neg && b->neg ? 1 : -1;
}

void bigint_neg(bigint* a) {
    a->neg = !a->neg;
}

static u64 add_wcarry(u64* a, u64 b) {
    *a += b;
    return *a < b;
}

static u64 sub_wcarry(u64* a, u64 b) {
    u64 tmp = *a;
    *a -= b;
    return *a > tmp;
}

// Only "adds" two bigints (doesn't take into account the `neg` flag).
void bigint_add_unsigned(bigint* a, const bigint* b) {
    usize na = buflen(a->d);
    usize nb = buflen(b->d);
    usize n = MAX(na, nb);
    while (buflen(a->d) != n) bufpush(a->d, 0);
    u64 carry = 0;

    usize i;
    for (i = 0; i < nb; i++) {
        carry =  add_wcarry(&a->d[i], carry);
        carry += add_wcarry(&a->d[i], b->d[i]);
    }
    for (; i < na && carry; i++) {
        carry = add_wcarry(&a->d[i], carry);
    }
    if (carry) bufpush(a->d, carry);
    bigint_normalize(a);
}

void bigint_sub_unsigned(bigint* a, const bigint* b) {
    u64 carry = 0;
    usize i;
    for (i = 0; i < buflen(b->d); i++) {
        carry =  sub_wcarry(&a->d[i], carry);
        carry += sub_wcarry(&a->d[i], b->d[i]);
    }
    for (; i < buflen(a->d) && carry; i++) {
        carry = sub_wcarry(&a->d[i], carry);
    }
    bigint_normalize(a);
}

void bigint_add_signed(bigint* a, bool aneg, const bigint* b, bool bneg) {
    if (a->neg == b->neg) {
        bigint_add_unsigned(a, b);
        a->neg = aneg;
    } else if (bigint_cmp_abs(a, b) >= 0) {
        bigint_sub_unsigned(a, b);
        a->neg = aneg;
    } else {
        bigint c = bigint_new();
        bigint_copy(&c, b);
        bigint_sub_unsigned(&c, a);
        bigint_free(a);
        *a = c;
    }
}

void bigint_add(bigint* a, const bigint* b) {
    bigint_add_signed(a, a->neg, b, b->neg);
}

void bigint_sub(bigint* a, const bigint* b) {
    bigint_add_signed(a, a->neg, b, !b->neg);
}

void bigint_shl(bigint* a) {
    u64 carry = 0;
    bufloop(a->d, i) {
        u64 word = a->d[i];
        u64 newcarry = (word & ((u64)1<<63)) >> 63;
        word <<= 1;
        word |= carry;
        carry = newcarry;
        a->d[i] = word;
    }
    if (carry) bufpush(a->d, carry);
    bigint_normalize(a);
}

void bigint_shln(bigint* a, usize n) {
    for (usize i = 0; i < n; i++) bigint_shl(a);
}

void bigint_shr(bigint* a) {
    u64 carry = 0;
    bufrevloop(a->d, i) {
        u64 word = a->d[i];
        u64 newcarry = word & 1;
        word >>= 1;
        word |= (carry<<63);
        carry = newcarry;
        a->d[i] = word;
    }
    bigint_normalize(a);
}

void bigint_shrn(bigint* a, usize n) {
    for (usize i = 0; i < n; i++) bigint_shr(a);
}

void bigint_set_bit(bigint* a, usize bit, bool set) {
    usize word_idx = bit / U64_BITS;
    usize bit_idx_in_word = bit % U64_BITS;

    while (buflen(a->d) < word_idx+1) bufpush(a->d, 0);
    u64 word = a->d[word_idx];
    u64 val = set ? 1 : 0;
    word |= val<<bit_idx_in_word;
    a->d[word_idx] = word;
    bigint_normalize(a);
}

static u64 word_mul_hi(u64 a, u64 b) {
    usize n = U64_BITS/2;
    u64 ahi = a >> n;
    u64 alo = a & WORD_HALF_MASK;
    u64 bhi = b >> n;
    u64 blo = b & WORD_HALF_MASK;
    u64 tmp = ((alo*blo) >> n) + ahi*blo;
    tmp = (tmp >> n) + ((alo*bhi + (tmp & WORD_HALF_MASK)) >> n);
    return tmp + ahi*bhi;
}

void bigint_mul(bigint* a, const bigint* b) {
    usize na = buflen(a->d);
    usize nb = buflen(b->d);
    usize nc = na + nb + 1;

    bigint c = bigint_new();
    c.neg = (int)a->neg ^ (int)b->neg;
    while (buflen(c.d) != nc) {
        bufpush(c.d, 0);
    }

    bigint carries = bigint_new();
    while (buflen(carries.d) != nc) {
        bufpush(carries.d, 0);
    }

    for (usize ia = 0; ia < na; ia++) {
        for (usize ib = 0; ib < nb; ib++) {
            usize i = ia + ib;
            usize j = i + 1;
            carries.d[i+1] += add_wcarry(&c.d[i], a->d[ia] * b->d[ib]);
            carries.d[j+1] += add_wcarry(
                &c.d[j],
                word_mul_hi(a->d[ia], b->d[ib]));
        }
    }
    bigint_add_unsigned(&c, &carries);

    bigint_free(&carries);
    bigint_free(a);
    *a = c;
    bigint_normalize(a);
}

void bigint_div_mod(const bigint* num, const bigint* den, bigint* quo, bigint* rem) {
    // TODO: error on division by zero
    bigint_copy(rem, num);
    bigint_set_u64(quo, 0);

    if (bigint_cmp_abs(rem, den) >= 0) {
        bigint ourden = bigint_new();
        bigint_copy(&ourden, den);

        int n = (int)(bigint_bitlength(rem) - bigint_bitlength(&ourden));
        bigint_shln(&ourden, (usize)n);

        for (; n >= 0; n--) {
            if (bigint_cmp_abs(rem, &ourden) >= 0) {
                bigint_sub_unsigned(rem, &ourden);
                bigint_set_bit(quo, n, true);
            }
            bigint_shr(&ourden);
        }

        bigint_free(&ourden);
    }
    quo->neg = (int)num->neg ^ (int)den->neg;
    rem->neg = rem->neg;

    bigint_normalize(quo);
    bigint_normalize(rem);
}

bool bigint_fits(const bigint* a, int bytes, bool signd) {
    if ((!signd && a->neg) || buflen(a->d) > 1) return false;
    if (buflen(a->d) == 0) return true;

    if (signd) {
        u64 max = maxinteger_signed(bytes);
        if (a->neg) {
            if (a->d[0] > max+1) return false;
            else return true;
        } else {
            if (a->d[0] > max) return false;
            else return true;
        }
    } else {
        if (u64_bitlength(a->d[0]) > (u64)(bytes*8)) return false;
        else return true;
    }
}

char* bigint_tostring(const bigint* a) {
    char* str = NULL;
    if (buflen(a->d) == 0) {
        bufpush(str, '0');
    } else {
        bigint tmp = bigint_new();
        bigint_copy(&tmp, a);
        bigint quo = bigint_new();
        bigint rem = bigint_new();
        bigint base = bigint_new_u64(10);
        char* alpha = "0123456789abcdef";

        while (buflen(tmp.d) > 0) {
            bigint_div_mod(&tmp, &base, &quo, &rem);
            bigint_copy(&tmp, &quo);
            bufpush(str, alpha[rem.d[0]]);
        }
        if (a->neg) bufpush(str, '-');

        usize len = buflen(str);
        usize halflen = len/2;
        for (usize i = 0; i < halflen; i++) {
            char c = str[len-1-i];
            str[len-1-i] = str[i];
            str[i] = c;
        }
    }

    bufpush(str, '\0');
    return str;
}

void test_bigint() {
    /*
    {
        bigint a = bigint_new_u64(UINT64_MAX);
        bigint_shln(&a, 3);
        bigint b = bigint_new_u64(50);
        bigint quo = bigint_new();
        bigint rem = bigint_new();
        bigint_div_mod(&a, &b, &quo, &rem);

        bigint ass = bigint_new_u64(2951479051793528258);
        assert(bigint_cmp_abs(&ass, &quo) == 0);
    }

    {
        bigint new = bigint_new();
        bufpush(new.d, 849348);
        bufpush(new.d, 0);
        bufpush(new.d, 0);
        bufpush(new.d, 0);
        bigint_normalize(&new);
        assert(buflen(new.d) == 1);
    }

    {
        bigint a = bigint_new_u64(UINT64_MAX);
        bigint b = bigint_new_u64(2);
        bigint_mul(&a, &b);
    }

    {
        bigint a = bigint_new_u64(UINT64_MAX);
        const bigint b = bigint_new_u64(UINT64_MAX);
        bigint_mul(&a, &b);
        bigint_mul(&a, &b);
        fprintf(stderr, "num: %s\n", bigint_tostring(&a));
    }

    {
        bigint a = bigint_new_u64(UINT64_MAX);
        assert(bigint_fits(&a, 8, false));
        a = bigint_new_u64(65536);
        assert(!bigint_fits(&a, 2, false));
        assert(bigint_fits(&a, 4, false));
        a = bigint_new_u64(128);
        a.neg = true;
        assert(bigint_fits(&a, 1, true));
    }
    */

    {
        bigint a = bigint_new();
        bufpush(a.d, 0b1001011101001100001011010111100000000000000000000000000000000000);
        bufpush(a.d, 0b1000001000000000000100011100011011000001010010110010111010);

        bigint b = bigint_new_u64(3);

        bigint quo = bigint_new();
        bigint rem = bigint_new();

        bigint_div_mod(&a, &b, &quo, &rem);
        assert(strcmp("900000000000000000000000000000000000", bigint_tostring(&quo)) == 0);
        assert(strcmp("0", bigint_tostring(&rem)) == 0);
    }
}
