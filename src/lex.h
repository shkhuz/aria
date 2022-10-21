#ifndef LEX_H
#define LEX_H

#include "core.h"
#include "types.h"

typedef struct {
    Srcfile* srcfile;
    const char* start, *current, *last_newl;
    usize line;
    bool error;
} LexContext;

LexContext lex_new_context(Srcfile* srcfile);
void lex(LexContext* l);

#endif
