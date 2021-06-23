// Defines how many "digits" to reserve when initializing BigInt.
#define BIGINT_PREC 1
#define BIGINT_DIGIT_BIT 60
#define BIGINT_MASK ((((u64)1)<<((u64)BIGINT_DIGIT_BIT))-((u64)1))
/* #define BIGINT_DIGIT_MAX BIGINT_MASK */

typedef struct {
	u64* d;
	bool negative;
} BigInt;

ErrorCode bigint_init(BigInt* self) {
	self->d = null;
	buf_fit(self->d, BIGINT_PREC);
	if (self->d == null) return ERROR_CODE_OUT_OF_MEM;
	
	for (size_t i = 0; i < BIGINT_PREC; i++) {
		self->d[i] = 0;
	}
	self->negative = false;
	return ERROR_CODE_OKAY;
}

void bigint_free(BigInt* self) {
	if (self->d != null) {
		for (size_t i = 0; i < buf_len(self->d); i++) {
			self->d[i] = 0;
		}

		buf_free(self->d);
		self->d = null;
		self->negative = false;
	}
}

ErrorCode bigint_grow(BigInt* self, size_t len) {
	size_t oldcap = buf_cap(self->d);
	buf_fit(self->d, len);
	for (size_t i = oldcap; i < buf_cap(self->d); i++) {
		self->d[i] = 0;
	}
	return ERROR_CODE_OKAY;
}

void bigint_clamp(BigInt* self) {
	while (buf_len(self->d) > 0 && self->d[buf_len(self->d)-1] == 0) {
		buf_pop(self->d);
	}
	if (buf_len(self->d) == 0) {
		self->negative = false;
	}
}

ErrorCode bigint_copy(BigInt* self, BigInt* dest) {
	if (self == dest) return ERROR_CODE_OKAY;
	if (buf_cap(dest->d) < buf_len(self->d)) {
		buf_fit(dest->d, buf_len(self->d));
	}

	{
		register u64* tmpself, *tmpdest;
		tmpself = self->d;
		tmpdest = dest->d;

		size_t n;
		for (n = 0; n < buf_len(self->d); n++) {
			*tmpdest++ = *tmpself++;
		}

		for (; n < buf_len(dest->d); n++) {
			*tmpdest++ = 0;
		}
	}

	_buf_hdr(dest->d)->len = _buf_hdr(self->d)->len;
	dest->negative = self->negative;
	return ERROR_CODE_OKAY;
}

void bigint_zero(BigInt* self) {
	u64* tmp;
	self->negative = false;
	buf_clear(self->d);

	tmp = self->d;
	for (size_t n = 0; n < buf_cap(self->d); n++) {
		*tmp++ = 0;
	}
}

ErrorCode bigint_abs(BigInt* self, BigInt* dest) {
	ErrorCode result;
	if (self != dest) {
		if ((result = bigint_copy(self, dest)) != ERROR_CODE_OKAY) {
			return result;
		}
	}
	dest->negative = false;
	return ERROR_CODE_OKAY;
}

/* ErrorCode bigint_negate(BigInt* self, BigInt* dest) { */
/* 	ErrorCode result; */
/* 	if (self != dest) { */
/* 		if ((result = bigint_copy(self, dest)) != ERROR_CODE_OKAY) { */
/* 			return result; */
/* 		} */
/* 	} */
	
/* 	if ( */
/* } */

ErrorCode bigint_init_unsigned(BigInt* self, u64 n) {
	ErrorCode result = bigint_init(self);
	if (n == 0) {
		return result;
	}
	buf_push(self->d, n);
	return result;
}

ErrorCode bigint_init_signed(BigInt* self, i64 n) {
	if (n >= 0) {
		return bigint_init_unsigned(self, n);
	}
	self->negative = true;
	buf_push(self->d, ((u64)(-(n + 1))) + 1);
	return ERROR_CODE_OKAY;
}

ErrorCode _bigint_add(BigInt* a, BigInt* b, BigInt* c) {
	BigInt* x;
	size_t oldlen, min, max;
	ErrorCode result;

	if (buf_len(a->d) > buf_len(b->d)) {
		min = buf_len(b->d);
		max = buf_len(a->d);
		x = a;
	} else {
		min = buf_len(a->d);
		max = buf_len(b->d);
		x = b;
	}
	buf_fit(c->d, max + 1);

	oldlen = buf_len(c->d);
	_buf_hdr(c->d)->len = max + 1;

	{
		register u64 u;
		register u64* tmpa, *tmpb, *tmpc;
		register size_t i;

		tmpa = a->d;
		tmpb = b->d;
		tmpc = c->d;

		u = 0;
		for (i = 0; i < min; i++) {
			*tmpc = *tmpa++ + *tmpb++ + u;
			u = *tmpc >> ((u64)BIGINT_DIGIT_BIT);
			*tmpc++ &= BIGINT_MASK;
		}

		if (min != max) {
			for (; i < max; i++) {
				*tmpc = x->d[i] + u;
				u = *tmpc >> ((u64)BIGINT_DIGIT_BIT);
				*tmpc++ &= BIGINT_MASK;
			}
		}

		*tmpc++ = u;
		for (i = buf_len(c->d); i < oldlen; i++) {
			*tmpc++ = 0;
		}
	}

	bigint_clamp(c);
	return ERROR_CODE_OKAY;
}
