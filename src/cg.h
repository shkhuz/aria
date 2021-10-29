#ifndef CG_H
#define CG_H

#include "core.h"
#include "aria.h"

typedef struct {
    Srcfile* srcfile;
    char* gencode;
    char* outpath;
    int indent;
} CgContext;

void cg(CgContext* c, char* finoutpath);
void cg_output_bin(CgContext* c, char* finoutpath);

#endif
