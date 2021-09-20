#ifndef PARSE_H
#define PARSE_H

#include "core.h"
#include "aria.h"

typedef struct {
    Srcfile* srcfile;
    size_t token_idx;
    bool error;
} ParseContext;

void parse(ParseContext* p);

#endif
