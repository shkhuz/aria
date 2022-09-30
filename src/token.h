#ifndef TOKEN_H
#define TOKEN_H

#include "core.h"
#include "types.h"
#include "bigint.h"

typedef struct Srcfile Srcfile;

typedef enum {
    TOKEN_KIND_IDENTIFIER,
    TOKEN_KIND_KEYWORD,
    TOKEN_KIND_STRING,
    TOKEN_KIND_INTEGER,
    TOKEN_KIND_LBRACE,
    TOKEN_KIND_RBRACE,
    TOKEN_KIND_LBRACK,
    TOKEN_KIND_RBRACK,
    TOKEN_KIND_LPAREN,
    TOKEN_KIND_RPAREN,
    TOKEN_KIND_COLON,
    TOKEN_KIND_SEMICOLON,
    TOKEN_KIND_DOT,
    TOKEN_KIND_COMMA,
    TOKEN_KIND_EQUAL,
    TOKEN_KIND_DOUBLE_EQUAL,
    TOKEN_KIND_BANG,
    TOKEN_KIND_BANG_EQUAL,
    TOKEN_KIND_LANGBR,
    TOKEN_KIND_LANGBR_EQUAL,
    TOKEN_KIND_RANGBR,
    TOKEN_KIND_RANGBR_EQUAL,
    TOKEN_KIND_AMP,
    TOKEN_KIND_DOUBLE_AMP,
    TOKEN_KIND_PLUS,
    TOKEN_KIND_MINUS,
    TOKEN_KIND_STAR,
    TOKEN_KIND_FSLASH,
    TOKEN_KIND_EOF,
} TokenKind;

typedef struct {
    TokenKind kind;
    char* start, *end;
    Srcfile* srcfile;
    usize line, col, ch_count;
    union {
        struct {
            bigint val;
        } integer;
    };
} Token;

#endif
