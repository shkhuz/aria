#ifndef _COMPILER_H
#define _COMPILER_H

#include <error_value.h>

typedef struct {
    const char* srcfile_path;
} Compiler;

Compiler compiler_new(const char* srcfile_path);
Error compiler_run(Compiler* self);

#endif /* _COMPILER_H */
