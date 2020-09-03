#ifndef _COMPILER_H
#define _COMPILER_H

typedef struct {
    const char* srcfile_path;
} Compiler;

Compiler compiler_new(const char* srcfile_path);
int compiler_run(Compiler* c);

#endif /* _COMPILER_H */
