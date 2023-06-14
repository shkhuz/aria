#ifndef AST_H
#define AST_H

#include "bigint.h"
#include "token.h"
#include "type.h"

typedef struct AstNode AstNode;
struct Typespec;

typedef struct {
    AstNode** params;
    AstNode* ret_typespec;
} AstNodeTypespecFunc;

typedef struct {
    bool immutable;
    AstNode* child;
} AstNodeTypespecPtr;

typedef struct {
    bool immutable;
    AstNode* child;
} AstNodeTypespecMultiptr;

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

typedef struct {
    Token* callee;
    AstNode** args;
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
    AstNode* ref;
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
    Token* rparen;
} AstNodeFunctionCall;

typedef struct {
    AstNode* left;
    AstNode* right;
} AstNodeAccess;

typedef enum {
    UNOP_NEG,
    UNOP_ADDR,
} UnopKind;

typedef struct {
    AstNode* child;
    UnopKind kind;
} AstNodeUnop;

typedef struct {
    AstNode* child;
} AstNodeDeref;

typedef struct {
    AstNode* left;
    AstNode* idx;
} AstNodeIndex;

typedef enum {
    BINOP_ADD,
    BINOP_SUB,
} BinopKind;

typedef struct {
    AstNode* left, *right;
    BinopKind kind;
} AstNodeBinop;

typedef struct {
    AstNode* left, *right;
} AstNodeAssign;

typedef struct {
    Token* arg;
    Typespec* mod_ty;
    char* name;
} AstNodeImport;

typedef struct {
    Span span;
    Token* identifier;
    char* name;
    AstNode** params;
    AstNode* ret_typespec;
} AstNodeFunctionHeader;

typedef struct {
    AstNode* header;
    AstNode* body;

    bool global;
} AstNodeFunctionDef;

typedef struct {
    Token* identifier;
    char* name;
    AstNode* typespec;
    Token* equal;
    AstNode* initializer;

    bool stack;
    bool immutable;
} AstNodeVariableDecl;

typedef struct {
    Token* identifier;
    char* name;
    AstNode* typespec;
} AstNodeParamDecl;

typedef struct {
    Token* identifier;
    char* name;
    AstNode** fields;
} AstNodeStruct;

typedef enum {
    ASTNODE_TYPESPEC_FUNC,
    ASTNODE_TYPESPEC_PTR,
    ASTNODE_TYPESPEC_MULTIPTR,
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
    ASTNODE_DEREF,
    ASTNODE_INDEX,
    ASTNODE_BINOP,
    ASTNODE_ASSIGN,
    ASTNODE_IMPORT,
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
    Span short_span;
    Typespec* typespec;
    union {
        AstNodeTypespecFunc typefunc;
        AstNodeTypespecPtr typeptr;
        AstNodeTypespecMultiptr typemulptr;
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
        AstNodeUnop unop;
        AstNodeDeref deref;
        AstNodeIndex idx;
        AstNodeBinop binop;
        AstNodeAssign assign;
        AstNodeImport import;
        AstNodeFunctionHeader funch;
        AstNodeFunctionDef funcdef;
        AstNodeVariableDecl vard;
        AstNodeParamDecl paramd;
        AstNode* exprstmt;
        AstNodeStruct strct;
    };
};

AstNode* astnode_typespec_func_new(Token* start, AstNode** params, AstNode* ret_typespec);
AstNode* astnode_typespec_ptr_new(Token* star, bool immutable, AstNode* child);
AstNode* astnode_typespec_multiptr_new(Token* start, bool immutable, AstNode* child);
AstNode* astnode_typespec_slice_new(Token* lbrack, bool immutable, AstNode* child);
AstNode* astnode_typespec_array_new(Token* lbrack, AstNode* size, AstNode* child);
AstNode* astnode_typespec_tuple_new(Token* start, AstNode** elems, Token* end);
AstNode* astnode_generic_typespec_new(AstNode* left, AstNode** args, Token* end);

AstNode* astnode_directive_new(
    Token* start,
    Token* callee,
    AstNode** args,
    Token* rparen);
AstNode* astnode_field_new(Token* key, AstNode* value);
AstNode* astnode_field_in_literal_new(Token* start, Token* key, AstNode* value);

AstNode* astnode_integer_literal_new(Token* token, bigint val);
AstNode* astnode_array_literal_new(Token* lbrack, AstNode** elems, Token* end);
AstNode* astnode_tuple_literal_new(Token* start, AstNode** elems, Token* end);
AstNode* astnode_aggregate_literal_new(AstNode* typespec, AstNode** fields, Token* end);

AstNode* astnode_symbol_new(Token* identifier);
AstNode* astnode_function_call_new(AstNode* callee, AstNode** args, Token* end);
AstNode* astnode_access_new(Token* op, AstNode* left, AstNode* right);
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

AstNode* astnode_unop_new(UnopKind kind, Token* op, AstNode* child);
AstNode* astnode_deref_new(AstNode* child, Token* dot, Token* star);
AstNode* astnode_index_new(AstNode* left, AstNode* idx, Token* end);
AstNode* astnode_binop_new(BinopKind kind, Token* op, AstNode* left, AstNode* right);
AstNode* astnode_assign_new(Token* equal, AstNode* left, AstNode* right);

AstNode* astnode_import_new(Token* keyword, Token* arg, struct Typespec* mod_ty, char* name);
AstNode* astnode_function_header_new(
    Token* start,
    Token* identifier,
    AstNode** params,
    AstNode* ret_typespec);
AstNode* astnode_function_def_new(AstNode* header, AstNode* body, bool global);
AstNode* astnode_variable_decl_new(
    Token* start,
    Token* identifier,
    AstNode* typespec,
    Token* equal,
    AstNode* initializer,
    bool stack,
    bool immutable);
AstNode* astnode_param_new(Token* identifier, AstNode* typespec);
AstNode* astnode_exprstmt_new(AstNode* expr);
AstNode* astnode_struct_new(
    Token* keyword,
    Token* identifier,
    AstNode** fields,
    Token* rbrace);

char* astnode_get_name(AstNode* astnode);
bool astnode_is_lvalue(AstNode* astnode);

#endif
