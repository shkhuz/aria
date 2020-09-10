#ifndef _COMPILER_H
#define _COMPILER_H

#include <parser.h>
#include <error_value.h>

typedef struct {
    const char* srcfile_path;
} Compiler;

typedef struct {
    Error error;
    Parser* parser;
} CompileOutput;

Compiler compiler_new(const char* srcfile_path);
CompileOutput compiler_run(Compiler* self);
void compiler_register_cache_decl(File* srcfile, Stmt** decls);

#endif /* _COMPILER_H */
