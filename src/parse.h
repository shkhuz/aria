#ifndef PARSE_H
#define PARSE_H

#include "core.h"
#include "types.h"

typedef struct {
    Srcfile* srcfile;
    usize token_idx;
    bool error;
} ParseContext;

ParseContext parse_new_context(Srcfile* srcfile);
void parse(ParseContext* p);

#endif
