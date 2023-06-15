#ifndef BIGINT_H
#define BIGINT_H

#include "core.h"

typedef struct {
    u64* d;
    bool neg;
} bigint;

extern bigint BIGINT_ZERO;

void init_bigint();
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

#endif
