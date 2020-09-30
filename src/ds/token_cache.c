#include "token.h"
#include "token_type.h"
#include "token_cache.h"

TokenCache token_cache;

void token_cache_init(void) {
    token_cache.equal = token_from_string_type("=", T_EQUAL);
}
