#ifndef TYPES_H
#define TYPES_H

#include "core.h"
#include "file_io.h"
#include "token.h"

typedef struct Srcfile {
    File handle;
    Token** tokens;
} Srcfile;

typedef struct {
    char* k;
    int v;
} StringIntMap;

extern char* g_exec_path;
extern StringIntMap* keywords;

void init_types();

#endif
