#ifndef SEMA_H
#define SEMA_H

#include "core.h"
#include "ast.h"
#include "type.h"

struct CompileCtx;
struct Srcfile;

typedef struct {
    Token* key;
    Typespec* value;
} TokenTypespecTup;

typedef struct {
    TokenTypespecTup* decls;
} ScopeInfo;

typedef struct {
    struct Srcfile* srcfile;
    struct CompileCtx* compile_ctx;
    ScopeInfo* scopebuf;
    bool error;
} SemaCtx;

SemaCtx sema_new_context(
    struct Srcfile* srcfile,
    struct CompileCtx* compile_ctx);
bool sema(SemaCtx* sema_ctxs);

#endif
