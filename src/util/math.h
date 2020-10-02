#ifndef _MATH_H
#define _MATH_H

#include "types.h"

/* --- NOTE: do not use expressions for arguments --- */
#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define CLAMP_MAX(x, max) MIN(x, max)
#define CLAMP_MIN(x, min) MAX(x, min)

u64 math_round_up(u64 num, u64 multiple);

#endif /* _MATH_H */
