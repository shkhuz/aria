#ifndef AST_H
#define AST_H

#include "misc.h"

typedef struct AstNode AstNode;

typedef struct {
    AstNode* child;
} AstNodeTypespecPtr;

typedef struct {
    AstNode* symbol;
} AstNodeTypespecSymbol;

typedef struct {
    Token* token;
    bigint val;
} AstNodeIntegerLiteral;

typedef struct {
    Token* identifier;
} AstNodeSymbol;

typedef struct {
    AstNode** stmts;
    AstNode* val;
} AstNodeScopedBlock;

typedef struct {
    AstNode* cond;
    AstNode* body;
} AstNodeIfBranch;

typedef struct {
    AstNode* ifbr;
    AstNode** elseifbr;
    AstNode* elsebr;
} AstNodeIf;

typedef struct {
    AstNode* operand;
} AstNodeReturn;

typedef struct {
    AstNode* callee;
    AstNode** args;
} AstNodeFunctionCall;

typedef enum {
    UNOP_NEG,
    UNOP_ADDR,
} UnOpKind;

typedef struct {
    AstNode* child;
    UnOpKind kind;
} AstNodeUnOp;

typedef enum {
    BINOP_ADD,
    BINOP_SUB,
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
    ASTNODE_TYPESPEC_SYMBOL,
    ASTNODE_TYPESPEC_PTR,
    ASTNODE_INTEGER_LITERAL,
    ASTNODE_SYMBOL,
    ASTNODE_SCOPED_BLOCK,
    ASTNODE_IF_BRANCH,
    ASTNODE_IF,
    ASTNODE_RETURN,
    ASTNODE_FUNCTION_CALL,
    ASTNODE_UNOP,
    ASTNODE_BINOP,
    ASTNODE_FUNCTION_HEADER,
    ASTNODE_FUNCTION_DEF,
    ASTNODE_VARIABLE_DECL,
    ASTNODE_PARAM_DECL,
} AstNodeKind;

struct AstNode {
    AstNodeKind kind;
    Span span;
    union {
        AstNodeTypespecPtr typeptr;
        AstNodeIntegerLiteral intl;
        // Also used for `ASTNODE_TYPESPEC_SYMBOL`
        AstNodeSymbol sym; 
        AstNodeScopedBlock blk;
        AstNodeIfBranch ifbr;
        AstNodeIf iff;
        AstNodeReturn ret;
        AstNodeFunctionCall funcc;
        AstNodeUnOp unop;
        AstNodeBinOp binop;
        AstNodeFunctionHeader funch;
        AstNodeFunctionDef funcd;
        AstNodeVariableDecl vard;
        AstNodeParamDecl paramd;
    };
};

AstNode* astnode_typespec_ptr_new(Token* star, AstNode* child);

AstNode* astnode_integer_literal_new(Token* token, bigint val);
AstNode* astnode_symbol_new(Token* identifier, bool is_typespec);
AstNode* astnode_function_call_new(AstNode* callee, AstNode** args, Token* end);
AstNode* astnode_scoped_block_new(
    Token* lbrace,
    AstNode** stmts,
    AstNode* val,
    Token* rbrace);
AstNode* astnode_if_branch_new(Token* keyword, AstNode* cond, AstNode* body);
AstNode* astnode_if_new(
    AstNode* ifbr,
    AstNode** elseifbr,
    AstNode* elsebr,
    AstNode* lastbr);
AstNode* astnode_return_new(Token* keyword, AstNode* operand);

AstNode* astnode_function_header_new(
    Token* start,
    Token* identifier,
    AstNode** params,
    AstNode* ret_type);
AstNode* astnode_function_def_new(AstNode* header, AstNode* body);
AstNode* astnode_variable_decl_new(
    Token* start,
    bool immutable,
    Token* identifier,
    AstNode* type,
    AstNode* initializer,
    Token* end);
AstNode* astnode_param_new(Token* identifier, AstNode* type);

#endif
