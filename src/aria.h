#ifndef ARIA_H
#define ARIA_H

#include "core.h"
#include "file_io.h"
#include "bigint.h"

#define TAB_SIZE 4

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
    TOKEN_KIND_INTEGER,
    TOKEN_KIND_LBRACE,
    TOKEN_KIND_RBRACE,
    TOKEN_KIND_LPAREN,
    TOKEN_KIND_RPAREN,
    TOKEN_KIND_COLON,
    TOKEN_KIND_SEMICOLON,
    TOKEN_KIND_COMMA,
    TOKEN_KIND_EQUAL,
    TOKEN_KIND_DOUBLE_EQUAL,
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
    union {
        struct {
            bigint* val;
        } integer;
    };
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
    BUILTIN_TYPE_KIND_BOOLEAN,
    BUILTIN_TYPE_KIND_VOID,
    _BUILTIN_TYPE_KIND_COUNT,
    BUILTIN_TYPE_KIND_APINT,
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
    Type* boolean;
    Type* void_type;
    Type* void_ptr;
} BuiltinTypePlaceholders;
extern BuiltinTypePlaceholders builtin_type_placeholders;

typedef struct {
    Token* token;
    BuiltinTypeKind kind;
    union {
        bigint* apint;
    };
} BuiltinType;

typedef struct {
    bool constant;
    Type* child;
} PtrType;

#define PTR_SIZE_BYTES 8

struct Type {
    TypeKind kind;
    Token* main_token;
    union {
        BuiltinType builtin;
        PtrType ptr;
    };
};

typedef enum {
    EXPR_KIND_INTEGER,
    EXPR_KIND_CONSTANT,
    EXPR_KIND_SYMBOL,
    EXPR_KIND_FUNCTION_CALL,
    EXPR_KIND_BINOP,
    EXPR_KIND_BLOCK,
    EXPR_KIND_IF,
    EXPR_KIND_WHILE,
} ExprKind;

typedef struct {
    Token* integer;
    bigint* val;
} IntegerExpr;

typedef enum {
    CONSTANT_KIND_BOOLEAN_TRUE,
    CONSTANT_KIND_BOOLEAN_FALSE,
    CONSTANT_KIND_NULL,
} ConstantKind;

typedef struct {
    Token* keyword;
    ConstantKind kind;
} ConstantExpr;

typedef struct {
    Token* identifier;
    Stmt* ref;
} SymbolExpr;

typedef struct {
    Expr* callee;
    Expr** args;
    Token* rparen;
} FunctionCallExpr;

typedef struct {
    Expr* left;
    Expr* right;
    Token* op;
    Type* left_type;
    Type* right_type;
} BinopExpr;

typedef struct {
    Stmt** stmts;
    Expr* value;
    Token* rbrace;
} BlockExpr;

typedef enum {
    IF_BRANCH_IF,
    IF_BRANCH_ELSEIF,
    IF_BRANCH_ELSE,
} IfBranchKind;

typedef struct {
    Expr* cond;
    Expr* body;
    IfBranchKind kind;
} IfBranch;

typedef struct {
    IfBranch* ifbr;
    IfBranch** elseifbr;
    IfBranch* elsebr;
} IfExpr;

typedef struct {
    Expr* cond;
    Expr* body;
} WhileExpr;

struct Expr {
    ExprKind kind;
    Token* main_token;
    Type* type;
    Stmt* parent_func;
    union {
        IntegerExpr integer;
        ConstantExpr constant;
        SymbolExpr symbol;
        FunctionCallExpr function_call;
        BinopExpr binop;
        BlockExpr block;
        IfExpr iff;
        WhileExpr whilelp;
    };
};

typedef enum {
    STMT_KIND_FUNCTION,
    STMT_KIND_VARIABLE,
    STMT_KIND_PARAM,
    STMT_KIND_ASSIGN,
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
    bool is_extern;
    size_t stack_vars_size;
    size_t ifidx;
    size_t whileidx;
} FunctionStmt;

typedef struct {
    bool constant;
    Token* identifier;
    Type* type;
    Type* initializer_type;
    Expr* initializer;
    size_t stack_offset;
} VariableStmt;

typedef struct {
    Token* identifier;
    Type* type;
    size_t idx;
    size_t stack_offset;
} ParamStmt;

typedef struct {
    Expr* left;
    Expr* right;
    Token* op;
} AssignStmt;

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
    Stmt* parent_func;
    union {
        FunctionStmt function;
        VariableStmt variable;
        ParamStmt param;
        AssignStmt assign;
        ReturnStmt return_stmt;
        ExprStmt expr;
    };
};

extern char** aria_keywords;

void init_ds();

bool is_token_lexeme_eq(Token* a, Token* b);
BuiltinTypeKind builtin_type_str_to_kind(char* str);
char* builtin_type_kind_to_str(BuiltinTypeKind kind);
bool builtin_type_is_integer(BuiltinTypeKind kind);
bool builtin_type_is_apint(BuiltinTypeKind kind);
bool builtin_type_is_signed(BuiltinTypeKind kind);
size_t builtin_type_bytes(BuiltinType* type);
Type* type_get_child(Type* type);
bool type_is_integer(Type* type);
bool type_is_apint(Type* type);
size_t type_bytes(Type* type);
Type* stmt_get_type(Stmt* stmt);

Type* builtin_type_new(Token* token, BuiltinTypeKind kind);
Type* ptr_type_new(Token* star, bool constant, Type* child);
FunctionHeader* function_header_new(
        Token* identifier, 
        Stmt** params, 
        Type* return_type);
Stmt* function_stmt_new(FunctionHeader* header, Expr* body, bool is_extern);
Stmt* variable_stmt_new(
        bool constant,
        Token* identifier,
        Type* type,
        Expr* initializer);
Stmt* param_stmt_new(Token* identifier, Type* type, size_t idx);
Stmt* assign_stmt_new(Expr* left, Expr* right, Token* op);
Stmt* expr_stmt_new(Expr* child);
Expr* integer_expr_new(Token* integer, bigint* val);
Expr* constant_expr_new(Token* keyword, ConstantKind kind);
Expr* symbol_expr_new(Token* identifier);
Expr* function_call_expr_new(Expr* callee, Expr** args, Token* rparen);
Expr* binop_expr_new(Expr* left, Expr* right, Token* op);
Expr* block_expr_new(
        Stmt** stmts, 
        Expr* value, 
        Token* lbrace, 
        Token* rbrace);
IfBranch* if_branch_new(Expr* cond, Expr* body, IfBranchKind kind);
Expr* if_expr_new(
        Token* if_keyword, 
        IfBranch* ifbr, 
        IfBranch** elseifbr, 
        IfBranch* elsebr);
Expr* while_expr_new(Token* while_keyword, Expr* cond, Expr* body);

void _aria_vfprintf(
        const char* calleefile, 
        size_t calleeline, 
        FILE* file, 
        const char* fmt, 
        va_list ap);

void _aria_fprintf(
        const char* calleefile, 
        size_t calleeline, 
        FILE* file, 
        const char* fmt, 
        ...);

#define aria_vfprintf(file, fmt, ap) \
    _aria_vfprintf( __FILE__, __LINE__, file, fmt, ap)

#define aria_fprintf(file, fmt, ...) \
    _aria_fprintf( __FILE__, __LINE__, file, fmt, ##__VA_ARGS__)

#define aria_printf(fmt, ...) \
    _aria_fprintf(__FILE__, __LINE__, stdout, fmt, ##__VA_ARGS__)

void fprintf_type(FILE* file, Type* type);
void fprintf_token(FILE* file, Token* token);

#endif
