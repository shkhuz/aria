#ifndef TYPES_H
#define TYPES_H

#include "core.h"
#include "file_io.h"
#include "bigint.h"

typedef struct Srcfile Srcfile;
typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef struct {
    char* k;
    int v;
} StringIntMap;

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
    const char* start, *end;
    Srcfile* srcfile;
    usize line, col, ch_count;
    union {
        struct {
            bigint val;
        } integer;
    };
} Token;

struct Srcfile {
    File handle;
    Token** tokens;
};

extern StringIntMap* keywords;

typedef enum {
    EXPR_KIND_NUMBER,
    EXPR_KIND_SYMBOL,
} ExprKind;

typedef struct {
    Token* loc;
    bigint val;
} ExprCompInteger;

typedef struct {
    Token* identifier;
    Stmt* ref;
} ExprSymbol;

typedef struct {
    Token* lbrace;
    Stmt** stmts;
    Expr* val;
} ExprScopedBlock;

typedef enum {
    UNOP_KIND_NEG,
    UNOP_KIND_ADDR,
} UnOpKind;

typedef struct {
    Expr* child;
    UnOpKind kind;
} ExprUnOp;

typedef enum {
    BINOP_KIND_ADD,
    BINOP_KIND_SUB,
} BinOpKind;

typedef struct {
    Expr* left, *right;
    BinOpKind kind;
} ExprBinOp;

struct Expr {
    ExprKind kind;
    Token* head, *tail;
    union {
        ExprCompInteger intg;
        ExprSymbol sym;
        ExprScopedBlock blk;
        ExprUnOp unop;
        ExprBinOp binop;
    };
};

struct Stmt {

};

void init_types();

#endif
