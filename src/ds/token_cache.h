#ifndef _TOKEN_CACHE_H
#define _TOKEN_CACHE_H

#include "token.h"

typedef struct {
    Token* equal;
} TokenCache;

extern TokenCache token_cache;

void token_cache_init(void);

#endif /* _TOKEN_CACHE_H */
