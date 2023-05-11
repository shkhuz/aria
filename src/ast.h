#ifndef AST_H
#define AST_H

#include "bigint.h"
#include "token.h"

typedef struct AstNode AstNode;

typedef struct {
    bool immutable;
    AstNode* child;
} AstNodeTypespecPtr;

typedef struct {
    AstNode* left;
    AstNode** args;
} AstNodeGenericTypespec;

typedef enum {
    DIRECTIVE_IMPORT,
    DIRECTIVE_NONE,
} DirectiveKind;

typedef struct {
    Token* callee;
    AstNode** args;
    DirectiveKind kind;
} AstNodeDirective;

typedef struct {
    Token* identifier;
    AstNode* typespec;
} AstNodeField;

typedef struct {
    Token* token;
    bigint val;
} AstNodeIntegerLiteral;

typedef struct {
    AstNode** elems;
} AstNodeTupleLiteral;

typedef struct {
    Token* identifier;
} AstNodeSymbol;

typedef struct {
    AstNode** stmts;
    AstNode* val;
} AstNodeScopedBlock;

typedef enum {
    IFBR_IF,
    IFBR_ELSEIF,
    IFBR_ELSE,
} IfBranchKind;

typedef struct {
    IfBranchKind kind;
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

typedef struct {
    AstNode* left;
    Token* right;
} AstNodeAccess;

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
    AstNode* ret_typespec;
} AstNodeFunctionHeader;

typedef struct {
    AstNode* header;
    AstNode* body;
} AstNodeFunctionDef;

typedef struct {
    bool immutable;
    Token* identifier;
    AstNode* typespec;
    AstNode* initializer;
} AstNodeVariableDecl;

typedef struct {
    Token* identifier;
    AstNode* typespec;
} AstNodeParamDecl;

typedef struct {
    Token* identifier;
    AstNode** fields;
} AstNodeStruct;

typedef enum {
    ASTNODE_TYPESPEC_PTR,
    ASTNODE_GENERIC_TYPESPEC,
    ASTNODE_DIRECTIVE,
    ASTNODE_FIELD,
    ASTNODE_INTEGER_LITERAL,
    ASTNODE_TUPLE_LITERAL,
    ASTNODE_SYMBOL,
    ASTNODE_SCOPED_BLOCK,
    ASTNODE_IF_BRANCH,
    ASTNODE_IF,
    ASTNODE_RETURN,
    ASTNODE_FUNCTION_CALL,
    ASTNODE_ACCESS,
    ASTNODE_UNOP,
    ASTNODE_BINOP,
    ASTNODE_FUNCTION_HEADER,
    ASTNODE_FUNCTION_DEF,
    ASTNODE_VARIABLE_DECL,
    ASTNODE_PARAM_DECL,
    ASTNODE_EXPRSTMT,
    ASTNODE_STRUCT,
} AstNodeKind;

struct AstNode {
    AstNodeKind kind;
    Span span;
    union {
        AstNodeTypespecPtr typeptr;
        AstNodeGenericTypespec genty;
        AstNodeDirective directive;
        AstNodeField field;
        AstNodeIntegerLiteral intl;
        AstNodeTupleLiteral tupl;
        AstNodeSymbol sym;
        AstNodeScopedBlock blk;
        AstNodeIfBranch ifbr;
        AstNodeIf iff;
        AstNodeReturn ret;
        AstNodeFunctionCall funcc;
        AstNodeAccess acc;
        AstNodeUnOp unop;
        AstNodeBinOp binop;
        AstNodeFunctionHeader funch;
        AstNodeFunctionDef funcd;
        AstNodeVariableDecl vard;
        AstNodeParamDecl paramd;
        AstNode* exprstmt;
        AstNodeStruct strct;
    };
};

AstNode* astnode_typespec_ptr_new(Token* star, bool immutable, AstNode* child);
AstNode* astnode_generic_typespec_new(AstNode* left, AstNode** args, Token* end);

AstNode* astnode_directive_new(
    Token* start,
    Token* callee,
    AstNode** args,
    DirectiveKind kind,
    Token* rparen);
AstNode* astnode_field_new(Token* identifier, AstNode* typespec);

AstNode* astnode_integer_literal_new(Token* token, bigint val);
AstNode* astnode_tuple_literal_new(Token* start, AstNode** elems, Token* end);
AstNode* astnode_symbol_new(Token* identifier);
AstNode* astnode_function_call_new(AstNode* callee, AstNode** args, Token* end);
AstNode* astnode_access_new(AstNode* left, Token* right);
AstNode* astnode_scoped_block_new(
    Token* lbrace,
    AstNode** stmts,
    AstNode* val,
    Token* rbrace);
AstNode* astnode_if_branch_new(
    Token* keyword,
    IfBranchKind kind,
    AstNode* cond,
    AstNode* body);
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
    AstNode* ret_typespec);
AstNode* astnode_function_def_new(AstNode* header, AstNode* body);
AstNode* astnode_variable_decl_new(
    Token* start,
    bool immutable,
    Token* identifier,
    AstNode* typespec,
    AstNode* initializer,
    Token* end);
AstNode* astnode_param_new(Token* identifier, AstNode* typespec);
AstNode* astnode_exprstmt_new(AstNode* expr);
AstNode* astnode_struct_new(
    Token* keyword,
    Token* identifier,
    AstNode** fields,
    Token* rbrace);

#endif
