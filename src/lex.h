#ifndef LEX_H
#define LEX_H

#include "core.h"
#include "aria.h"

typedef struct {
    Srcfile* srcfile;
    char* start, *current, *last_newline;
    size_t line;
    bool error;
} LexContext;

void lex(LexContext* l);

#endif
