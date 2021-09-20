#ifndef ARIA_H
#define ARIA_H

#include "core.h"
#include "file_io.h"

typedef struct Token Token;
typedef struct Type Type;
typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef struct {
    File* handle;
    Token** tokens;
    Stmt** stmts;
} Srcfile;

typedef enum {
    TOKEN_KIND_IDENTIFIER,
    TOKEN_KIND_KEYWORD,
    TOKEN_KIND_LBRACE,
    TOKEN_KIND_RBRACE,
    TOKEN_KIND_LPAREN,
    TOKEN_KIND_RPAREN,
    TOKEN_KIND_COLON,
    TOKEN_KIND_COMMA,
    TOKEN_KIND_PLUS,
    TOKEN_KIND_MINUS,
    TOKEN_KIND_STAR,
    TOKEN_KIND_FSLASH,
    TOKEN_KIND_EOF,
} TokenKind;

struct Token {
    TokenKind kind;
    char* lexeme;
    char* start, *end;
    Srcfile* srcfile;
    size_t line, column, char_count;
};

typedef enum {
    TYPE_KIND_BUILTIN,
    TYPE_KIND_PTR,
} TypeKind;

typedef enum {
    BUILTIN_TYPE_KIND_U8,
    BUILTIN_TYPE_KIND_U16,
    BUILTIN_TYPE_KIND_U32,
    BUILTIN_TYPE_KIND_U64,
    BUILTIN_TYPE_KIND_USIZE,
    BUILTIN_TYPE_KIND_I8,
    BUILTIN_TYPE_KIND_I16,
    BUILTIN_TYPE_KIND_I32,
    BUILTIN_TYPE_KIND_I64,
    BUILTIN_TYPE_KIND_ISIZE,
    BUILTIN_TYPE_KIND_VOID,
    _BUILTIN_TYPE_KIND_COUNT,
    BUILTIN_TYPE_KIND_NONE,
} BuiltinTypeKind;

typedef struct {
    char* k;
    BuiltinTypeKind v;
} BuiltinTypeMap;
extern BuiltinTypeMap builtin_type_map[_BUILTIN_TYPE_KIND_COUNT];

typedef struct {
    Type* u8;
    Type* u16;
    Type* u32;
    Type* u64;
    Type* usize;
    Type* i8;
    Type* i16;
    Type* i32;
    Type* i64;
    Type* isize;
    Type* void_type;
} BuiltinTypePlaceholders;
extern BuiltinTypePlaceholders builtin_type_placeholders;

typedef struct {
    Token* token;
    BuiltinTypeKind kind;
} BuiltinType;

typedef struct {
    bool constant;
    Type* child;
} PtrType;

#define PTR_SIZE 8

struct Type {
    TypeKind kind;
    Token* main_token;
    union {
        BuiltinType builtin;
        PtrType ptr;
    };
};

typedef enum {
    EXPR_KIND_SYMBOL,
    EXPR_KIND_BLOCK,
} ExprKind;

typedef struct {
    Token* identifier;
    Stmt* ref;
} SymbolExpr;

typedef struct {
    Token* lbrace;
    Stmt** stmts;
    Expr* value;
} BlockExpr;

struct Expr {
    ExprKind kind;
    Token* main_token;
    union {
        SymbolExpr symbol;
        BlockExpr block;
    };
};

typedef enum {
    STMT_KIND_FUNCTION,
    STMT_KIND_VARIABLE,
    STMT_KIND_PARAM,
    STMT_KIND_RETURN,
    STMT_KIND_EXPR,
} StmtKind;

typedef struct {
    Token* identifier;
    Stmt** params;
    Type* return_type;
} FunctionHeader;

typedef struct {
    FunctionHeader* header;
    Expr* body;
    size_t stack_vars_size;
} FunctionStmt;

typedef struct {
    bool constant;
    Token* identifier;
    Type* type;
    Expr* initializer;
    Stmt* parent_func;
} VariableStmt;

typedef struct {
    Token* identifier;
    Type* type;
} ParamStmt;

typedef struct {
    Expr* child;
    Stmt* parent_func;
} ReturnStmt;

typedef struct {
    Expr* child;
} ExprStmt;

struct Stmt {
    StmtKind kind;
    Token* main_token;
    union {
        FunctionStmt function;
        VariableStmt variable;
        ParamStmt param;
        ReturnStmt return_stmt;
        ExprStmt expr;
    };
};

extern char** aria_keywords;

void init_ds();

BuiltinTypeKind builtin_type_str_to_kind(char* str);

Type* builtin_type_new(Token* token, BuiltinTypeKind kind);
Type* ptr_type_new(Token* star, bool constant, Type* child);
FunctionHeader* function_header_new(
        Token* identifier, 
        Stmt** params, 
        Type* return_type);
Stmt* function_stmt_new(FunctionHeader* header, Expr* body);
Stmt* param_stmt_new(Token* identifier, Type* type);
Expr* block_expr_new(Token* lbrace, Stmt** stmts, Expr* value);

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
