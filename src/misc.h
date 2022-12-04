#ifndef MISC_H
#define MISC_H

#include "core.h"
#include "file_io.h"
#include "bigint.h"
#include "token.h"

typedef struct {
    char* k;
    TokenKind v;
} StringKwKind;

struct AstNode;

typedef struct Srcfile Srcfile;

struct Srcfile {
    File handle;
    Token** tokens;
    struct AstNode** astnodes;
};

extern StringKwKind* keywords;

void init_types();

char* aria_format(const char* fmt, ...);

#endif
