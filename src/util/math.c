#include "math.h"
#include <arpch.h>

u64 math_round_up(u64 num, u64 multiple) {
    assert(multiple);
    return ((num + multiple - 1) / multiple) * multiple;
}
