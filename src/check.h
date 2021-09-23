#ifndef CHECK_H
#define CHECK_H

#include "core.h"
#include "aria.h"

typedef struct {
    Srcfile* srcfile;
    bool error;
} CheckContext;

void check(CheckContext* c);

#endif
