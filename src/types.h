#ifndef TYPES_H
#define TYPES_H

#include "file_io.h"
#include "token.h"

typedef struct Srcfile {
    File handle;
    Token** tokens;
} Srcfile;

extern char* g_exec_path;

#endif
