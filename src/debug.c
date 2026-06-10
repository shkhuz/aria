#include "core.h"
#include "token.h"
#include "debug.h"

void print_tokens(Token** tokens) {
    bufloop(tokens, i) {
        printf(
            "%20s %s\n", 
            tokenkind_strs[tokens[i]->kind], 
            span_tostring(tokens[i]->span)
        );
    }
    printf("tokens: %lu\n", buflen(tokens));
}
