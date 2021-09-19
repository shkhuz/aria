#ifndef ARIA_H
#define ARIA_H

#include "core.h"
#include "file_io.h"

typedef struct Token Token;
typedef struct Stmt Stmt;

typedef struct {
    File* handle;
    Token** tokens;
    Stmt** stmts;
} Srcfile;

typedef enum {
    TOKEN_KIND_IDENTIFIER,
    TOKEN_KIND_KEYWORD,
    TOKEN_KIND_PLUS,
    TOKEN_KIND_MINUS,
    TOKEN_KIND_EOF,
} TokenKind;

struct Token {
    TokenKind kind;
    char* lexeme;
    char* start, *end;
    Srcfile* srcfile;
    size_t line, column, char_count;
};

typedef struct {
    char* name;
    bool ptr;
} Type;

struct Stmt {
};

extern char** aria_keywords;

void init_keywords();

void _aria_fprintf(
        const char* calleefile, 
        size_t calleeline, 
        FILE* file, 
        const char* fmt, 
        ...);

#define aria_fprintf(file, fmt, ...) \
    _aria_fprintf( __FILE__, __LINE__, file, fmt, ##__VA_ARGS__)

#define aria_printf(fmt, ...) \
    _aria_fprintf(__FILE__, __LINE__, stdout, fmt, ##__VA_ARGS__)

void fprint_type(FILE* file, Type* type);
void fprintf_token(FILE* file, Token* token);

#endif
