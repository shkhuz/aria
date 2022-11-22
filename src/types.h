#ifndef TYPES_H
#define TYPES_H

#include "core.h"
#include "file_io.h"
#include "bigint.h"
#include "span.h"

typedef struct Srcfile Srcfile;
typedef struct Type Type;
typedef struct AstNode AstNode;

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
} Token;

struct Srcfile {
    File handle;
    Token** tokens;
    AstNode** astnodes;
};

extern StringIntMap* keywords;

typedef enum {
    TYPE_KIND_PRIMITIVE,
    TYPE_KIND_PTR,
    TYPE_KIND_CUSTOM,
} TypeKind;

typedef enum {
    TYPE_PRIM_U8,
    TYPE_PRIM_U16,
    TYPE_PRIM_U32,
    TYPE_PRIM_U64,
    TYPE_PRIM_I8,
    TYPE_PRIM_I16,
    TYPE_PRIM_I32,
    TYPE_PRIM_I64,
    TYPE_PRIM_VOID,
    TYPE_PRIM_NONE,
} TypePrimitiveKind;

typedef struct {
    TypePrimitiveKind kind;
} TypePrimitive;

typedef struct {
} TypePtr;

typedef struct {
    Token* name;
} TypeCustom;

struct Type {
    TypeKind kind;
    union {
        TypePrimitive prim;
        TypePtr ptr;
        TypeCustom custom;
    };
};

typedef struct {
    Type u8_type;
    Type u16_type;
    Type u32_type;
    Type u64_type;
    Type i8_type;
    Type i16_type;
    Type i32_type;
    Type i64_type;
    Type void_type;
} TypePlaceholders;

extern TypePlaceholders type_placeholders;

typedef struct {
    Type type;
} AstNodeType;

typedef struct {
    Token* tok;
    bigint val;
} AstNodeIntegerLiteral;

typedef struct {
    Token* identifier;
    AstNode* ref;
} AstNodeSymbol;

typedef struct {
    AstNode** stmts;
    AstNode* val;
} AstNodeScopedBlock;

typedef enum {
    UNOP_KIND_NEG,
    UNOP_KIND_ADDR,
} UnOpKind;

typedef struct {
    AstNode* child;
    UnOpKind kind;
} AstNodeUnOp;

typedef enum {
    BINOP_KIND_ADD,
    BINOP_KIND_SUB,
} BinOpKind;

typedef struct {
    AstNode* left, *right;
    BinOpKind kind;
} AstNodeBinOp;

typedef struct {
    Span span;
    Token* identifier;
    AstNode** params;
    AstNode* ret_type;
} AstNodeFunctionHeader;

typedef struct {
    AstNode* header;
    AstNode* body;
} AstNodeFunctionDef;

typedef struct {
    bool immutable;
    Token* identifier;
    AstNode* type;
    AstNode* initializer;
} AstNodeVariableDecl;

typedef struct {
    Token* identifier;
    AstNode* type;
} AstNodeParamDecl;

typedef enum {
    ASTNODE_KIND_TYPE,
    ASTNODE_KIND_INTEGER_LITERAL,
    ASTNODE_KIND_SYMBOL,
    ASTNODE_KIND_SCOPED_BLOCK,
    ASTNODE_KIND_UNOP,
    ASTNODE_KIND_BINOP,
    ASTNODE_KIND_FUNCTION_HEADER,
    ASTNODE_KIND_FUNCTION_DEF,
    ASTNODE_KIND_VARIABLE_DECL,
    ASTNODE_KIND_PARAM_DECL,
} AstNodeKind;

struct AstNode {
    AstNodeKind kind;
    Span span;
    union {
        AstNodeType type;
        AstNodeIntegerLiteral intl;
        AstNodeSymbol sym;
        AstNodeScopedBlock blk;
        AstNodeUnOp unop;
        AstNodeBinOp binop;
        AstNodeFunctionHeader funch;
        AstNodeFunctionDef funcd;
        AstNodeVariableDecl vard;
        AstNodeParamDecl paramd;
    };
};

void init_types();

Token* token_new(TokenKind kind, Span span);
bool is_token_lexeme(Token* token, const char* string);

Type type_primitive_init(TypePrimitiveKind kind);
Type type_custom_init(Token* name);

AstNode* astnode_type_new(Type type, Span span);
AstNode* astnode_scoped_block_new(
    Token* lbrace,
    AstNode** stmts,
    AstNode* val,
    Token* rbrace);
AstNode* astnode_function_header_new(
    Token* keyword,
    Token* identifier,
    AstNode** params,
    AstNode* ret_type);
AstNode* astnode_function_def_new(AstNode* header, AstNode* body);
AstNode* astnode_param_new(Token* identifier, AstNode* type);

#endif
