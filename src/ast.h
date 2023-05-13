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
    bool immutable;
    AstNode* child;
} AstNodeTypespecSlice;

typedef struct {
    AstNode* size;
    AstNode* child;
} AstNodeTypespecArray;

typedef struct {
    AstNode** elems;
} AstNodeTypespecTuple;

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
    Token* key;
    AstNode* value;
} AstNodeField;

typedef struct {
    Token* token;
    bigint val;
} AstNodeIntegerLiteral;

typedef struct {
    AstNode* typespec;
    AstNode** elems;
} AstNodeArrayLiteral;

typedef struct {
    AstNode** elems;
} AstNodeTupleLiteral;

typedef struct {
    AstNode* typespec;
    AstNode** fields;
} AstNodeAggregateLiteral;

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
    AstNode* right;
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
    IMPL_AGGREGATE,
    IMPL_TRAIT,
} ImplKind;

typedef struct {
    ImplKind implkind;
    // NULL in case of IMPL_AGGREGATE
    AstNode* trait;
    AstNode* typespec;
    AstNode** children;
} AstNodeImpl;

typedef enum {
    ASTNODE_TYPESPEC_PTR,
    ASTNODE_TYPESPEC_SLICE,
    ASTNODE_TYPESPEC_ARRAY,
    ASTNODE_TYPESPEC_TUPLE,
    ASTNODE_GENERIC_TYPESPEC,
    ASTNODE_DIRECTIVE,
    ASTNODE_FIELD,
    ASTNODE_INTEGER_LITERAL,
    ASTNODE_ARRAY_LITERAL,
    ASTNODE_TUPLE_LITERAL,
    ASTNODE_AGGREGATE_LITERAL,
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
    ASTNODE_IMPL,
} AstNodeKind;

struct AstNode {
    AstNodeKind kind;
    Span span;
    union {
        AstNodeTypespecPtr typeptr;
        AstNodeTypespecSlice typeslice;
        AstNodeTypespecArray typearray;
        AstNodeTypespecTuple typetup;
        AstNodeGenericTypespec genty;
        AstNodeDirective directive;
        AstNodeField field;
        AstNodeIntegerLiteral intl;
        AstNodeArrayLiteral arrayl;
        AstNodeTupleLiteral tupl;
        AstNodeAggregateLiteral aggl;
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
        AstNodeImpl impl;
    };
};

AstNode* astnode_typespec_ptr_new(Token* star, bool immutable, AstNode* child);
AstNode* astnode_typespec_slice_new(Token* lbrack, bool immutable, AstNode* child);
AstNode* astnode_typespec_array_new(Token* lbrack, AstNode* size, AstNode* child);
AstNode* astnode_typespec_tuple_new(Token* start, AstNode** elems, Token* end);
AstNode* astnode_generic_typespec_new(AstNode* left, AstNode** args, Token* end);

AstNode* astnode_directive_new(
    Token* start,
    Token* callee,
    AstNode** args,
    DirectiveKind kind,
    Token* rparen);
AstNode* astnode_field_new(Token* key, AstNode* value);
AstNode* astnode_field_in_literal_new(Token* start, Token* key, AstNode* value);

AstNode* astnode_integer_literal_new(Token* token, bigint val);
AstNode* astnode_array_literal_new(AstNode* typespec, AstNode** elems, Token* end);
AstNode* astnode_tuple_literal_new(Token* start, AstNode** elems, Token* end);
AstNode* astnode_aggregate_literal_new(AstNode* typespec, AstNode** fields, Token* end);

AstNode* astnode_symbol_new(Token* identifier);
AstNode* astnode_function_call_new(AstNode* callee, AstNode** args, Token* end);
AstNode* astnode_access_new(AstNode* left, AstNode* right);
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
AstNode* astnode_impl_new(
    Token* keyword,
    ImplKind implkind,
    AstNode* trait,
    AstNode* typespec,
    AstNode** children,
    Token* rbrace);

#endif
