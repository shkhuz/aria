#ifndef TYPES_H
#define TYPES_H

#include "core.h"
#include "file_io.h"
#include "bigint.h"
#include "span.h"

typedef struct Srcfile Srcfile;
typedef struct Type Type;
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
    TOKEN_KIND_INTEGER_LITERAL,
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
    Span span;
    union {
        struct {
            bigint val;
        } intg;
    };
} Token;

struct Srcfile {
    File handle;
    Token** tokens;
    Stmt** stmts;
};

extern StringIntMap* keywords;

typedef enum {
    TYPE_KIND_PRIMITIVE,
    TYPE_KIND_PTR,
} TypeKind;

typedef struct {
} TypePrimitive;

typedef struct {
} TypePtr;

struct Type {
    TypeKind kind;
    union {
        TypePrimitive prim;
        TypePtr ptr;
    };
};

typedef enum {
    EXPR_KIND_TYPE,
    EXPR_KIND_INTEGER_LITERAL,
    EXPR_KIND_SYMBOL,
    EXPR_KIND_UNOP,
    EXPR_KIND_BINOP,
} ExprKind;

typedef struct {
    Type type;
} ExprType;

typedef struct {
    Token* tok;
    // bigint value is stored inside token
} ExprIntegerLiteral;

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
    Span span;
    union {
        ExprType type;
        ExprIntegerLiteral intl;
        ExprSymbol sym;
        ExprScopedBlock blk;
        ExprUnOp unop;
        ExprBinOp binop;
    };
};

typedef enum {
    STMT_KIND_FUNCTION_DEF,
    STMT_KIND_VARIABLE_DECL,
    STMT_KIND_PARAM,
    STMT_KIND_EXPR,
} StmtKind;

typedef struct {
    Span span;
    Token* identifier;
    Stmt** params;
    Expr* ret_type;
} FunctionHeader;

typedef struct {
    FunctionHeader header;
    Expr* body;
} StmtFunctionDef;

typedef struct {
    bool immutable;
    Token* identifier;
    Expr* type;
    Expr* initializer;
} StmtVariableDecl;

typedef struct {
    Token* identifier;
    Expr* type;
} StmtParam;

typedef struct {
    Expr* child;
} StmtExpr;

struct Stmt {
    StmtKind kind;
    Span span;
    union {
        StmtFunctionDef funcd;
        StmtVariableDecl vard;
        StmtParam param;
        StmtExpr expr;
    };
};

void init_types();
Token* token_new(TokenKind kind, Span span);
Stmt* stmt_function_def_new(FunctionHeader header, Expr* body);
Stmt* stmt_param_new(Token* identifier, Expr* type);

#endif
