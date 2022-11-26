#ifndef PARSE_H
#define PARSE_H

#include "core.h"
#include "types.h"

typedef struct {
    Srcfile* srcfile;
    Token* current;
    Token* prev;
    usize token_idx;
    bool error;
} ParseCtx;

ParseCtx parse_new_context(Srcfile* srcfile);
void parse(ParseCtx* p);

#endif
