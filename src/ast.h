#ifndef AST_H
#define AST_H

#include "bigint.h"
#include "token.h"
#include "type.h"

#include <llvm-c/Core.h>

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
    usize idx;
} AstNodeField;

typedef struct {
    Token* token;
    bigint val;
} AstNodeIntegerLiteral;

typedef struct {
    Token* token;
} AstNodeStringLiteral;

typedef struct {
    Token* token;
} AstNodeCharLiteral;

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

typedef enum {
    BS_u8,
    BS_u16,
    BS_u32,
    BS_u64,
    BS_i8,
    BS_i16,
    BS_i32,
    BS_i64,
    BS_bool,
    BS_void,
    BS_noreturn,
    BS_true,
    BS_false,
} BuiltinSymbolKind;

typedef struct {
    Token* identifier;
    BuiltinSymbolKind kind;
} AstNodeBuiltinSymbol;

typedef struct {
    AstNode** stmts;
    Token* yield_keyword;
    AstNode* val;
    Token* rbrace;
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
    AstNode* cond;
    AstNode* mainbody;
    AstNode* elsebody;

    // Stores references to
    // `break` expressions.
    AstNode** breaks;
    Typespec* target;
} AstNodeWhile;

typedef struct {
    // Assign or vardecls
    AstNode** decls;
    AstNode* cond;
    AstNode** counts;

    AstNode* mainbody;
    AstNode* elsebody;

    // Stores references to
    // `break` expressions.
    AstNode** breaks;
    Typespec* target;
} AstNodeCFor;

typedef struct {
    AstNode* child;
    AstNode* loopref;
    LLVMBasicBlockRef llvmbb;
} AstNodeBreak;

typedef struct {
} AstNodeContinue;

typedef struct {
    AstNode* child;
    AstNode* ref;
} AstNodeReturn;

typedef struct {
    AstNode* callee;
    AstNode** args;
    Token* rparen;
    // Points to the actual function, as opposed to
    // the identifier or the node before the `(`.
    // NULL if the callee is a function pointer.
    AstNode* ref;
} AstNodeFunctionCall;

typedef enum {
    SLICE_FIELD_PTR,
    SLICE_FIELD_LEN,
} SliceField;

typedef struct {
    AstNode* left;
    AstNode* right;
    AstNode* accessed;
    union {
        SliceField slicefield;
    };
} AstNodeAccess;

typedef enum {
    UNOP_NEG,
    UNOP_BOOLNOT,
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
    ARITH_BINOP_ADD,
    ARITH_BINOP_SUB,
    ARITH_BINOP_MUL,
    ARITH_BINOP_DIV,
    ARITH_BINOP_REM,
} ArithBinopKind;

typedef struct {
    AstNode* left, *right;
    ArithBinopKind kind;
    bool ptrop;
} AstNodeArithBinop;

typedef enum {
    BOOL_BINOP_AND,
    BOOL_BINOP_OR,
} BoolBinopKind;

typedef struct {
    AstNode* left, *right;
    BoolBinopKind kind;
} AstNodeBoolBinop;

typedef enum {
    CMP_BINOP_EQ,
    CMP_BINOP_NE,
    CMP_BINOP_LT,
    CMP_BINOP_GT,
    CMP_BINOP_LE,
    CMP_BINOP_GE,
} CmpBinopKind;

bool is_equality_op(CmpBinopKind kind);

typedef struct {
    AstNode* left, *right;
    CmpBinopKind kind;
    Typespec* peerres;
} AstNodeCmpBinop;

typedef struct {
    AstNode* left, *right;
} AstNodeAssign;

typedef struct {
    AstNode* left, *right;
} AstNodeCast;

typedef struct {
    Token* arg;
    Typespec* mod_ty;
    char* name;
} AstNodeImport;

typedef struct {
    Span span;
    Token* identifier;
    char* name;
    char* mangled_name;
    AstNode** params;
    AstNode* ret_typespec;
} AstNodeFunctionHeader;

typedef struct {
    AstNode* header;
    AstNode* body;
    bool export;

    AstNode** locals;
} AstNodeFunctionDef;

typedef struct {
    AstNode* header;
} AstNodeExternFunction;

typedef struct {
    Token* identifier;
    char* name;
    // Only for globals
    // NULL for locals
    char* mangled_name;
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
    bool immutable;
} AstNodeExternVariable;

typedef struct {
    Token* identifier;
    char* name;
    AstNode* typespec;
} AstNodeParamDecl;

typedef enum {
    CCWHITE,
    CCGREY,
    CCBLACK,
} CycleColor;

typedef struct {
    Token* identifier;
    char* name;
    char* mangled_name;
    AstNode** fields;
    LLVMTypeRef llvmtype;
    // Used for checking aggregate dependencies.
    AstNode** deps_on;
    CycleColor color;
    // Only for immediate children, not
    // set for nested aggregates.
    bool contains_array;
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
    ASTNODE_STRING_LITERAL,
    ASTNODE_CHAR_LITERAL,
    ASTNODE_ARRAY_LITERAL,
    ASTNODE_TUPLE_LITERAL,
    ASTNODE_AGGREGATE_LITERAL,
    ASTNODE_SYMBOL,
    ASTNODE_BUILTIN_SYMBOL,
    ASTNODE_SCOPED_BLOCK,
    ASTNODE_IF_BRANCH,
    ASTNODE_IF,
    ASTNODE_WHILE,
    ASTNODE_CFOR,
    ASTNODE_BREAK,
    ASTNODE_CONTINUE,
    ASTNODE_RETURN,
    ASTNODE_FUNCTION_CALL,
    ASTNODE_ACCESS,
    ASTNODE_UNOP,
    ASTNODE_DEREF,
    ASTNODE_INDEX,
    ASTNODE_ARITH_BINOP,
    ASTNODE_BOOL_BINOP,
    ASTNODE_CMP_BINOP,
    ASTNODE_ASSIGN,
    ASTNODE_CAST,
    ASTNODE_IMPORT,
    ASTNODE_FUNCTION_HEADER,
    ASTNODE_FUNCTION_DEF,
    ASTNODE_EXTERN_FUNCTION,
    ASTNODE_VARIABLE_DECL,
    ASTNODE_EXTERN_VARIABLE,
    ASTNODE_PARAM_DECL,
    ASTNODE_EXPRSTMT,
    ASTNODE_STRUCT,
} AstNodeKind;

struct AstNode {
    AstNodeKind kind;
    Span span;
    Span short_span;
    Typespec* typespec;

    LLVMValueRef llvmvalue;

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
        AstNodeStringLiteral strl;
        AstNodeCharLiteral cl;
        AstNodeArrayLiteral arrayl;
        AstNodeTupleLiteral tupl;
        AstNodeAggregateLiteral aggl;
        AstNodeSymbol sym;
        AstNodeBuiltinSymbol bsym;
        AstNodeScopedBlock blk;
        AstNodeIfBranch ifbr;
        AstNodeIf iff;
        AstNodeWhile whloop;
        AstNodeCFor cfor;
        AstNodeBreak brk;
        AstNodeContinue cont;
        AstNodeReturn ret;
        AstNodeFunctionCall funcc;
        AstNodeAccess acc;
        AstNodeUnop unop;
        AstNodeDeref deref;
        AstNodeIndex idx;
        AstNodeArithBinop arthbin;
        AstNodeBoolBinop boolbin;
        AstNodeCmpBinop cmpbin;
        AstNodeAssign assign;
        AstNodeCast cast;
        AstNodeImport import;
        AstNodeFunctionHeader funch;
        AstNodeFunctionDef funcdef;
        AstNodeExternFunction extfunc;
        AstNodeVariableDecl vard;
        AstNodeExternVariable extvar;
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
AstNode* astnode_field_new(Token* key, AstNode* value, usize idx);
AstNode* astnode_field_in_literal_new(Token* start, Token* key, AstNode* value);

AstNode* astnode_integer_literal_new(Token* token, bigint val);
AstNode* astnode_string_literal_new(Token* token);
AstNode* astnode_char_literal_new(Token* token);
AstNode* astnode_array_literal_new(Token* lbrack, AstNode** elems, Span end);
AstNode* astnode_tuple_literal_new(Token* start, AstNode** elems, Token* end);
AstNode* astnode_aggregate_literal_new(AstNode* typespec, AstNode** fields, Token* end);

AstNode* astnode_symbol_new(Token* identifier);
AstNode* astnode_builtin_symbol_new(BuiltinSymbolKind kind, Token* identifier);
AstNode* astnode_function_call_new(AstNode* callee, AstNode** args, Token* end);
AstNode* astnode_access_new(Token* op, AstNode* left, AstNode* right);
AstNode* astnode_scoped_block_new(
    Token* lbrace,
    AstNode** stmts,
    Token* yield_keyword,
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
AstNode* astnode_while_new(
    Token* keyword,
    AstNode* cond,
    AstNode* mainbody,
    AstNode* elsebody,
    AstNode** breaks);
AstNode* astnode_cfor_new(
    Token* keyword,
    AstNode** decls,
    AstNode* cond,
    AstNode** counts,
    AstNode* mainbody,
    AstNode* elsebody,
    AstNode** breaks);
AstNode* astnode_break_new(Token* keyword, AstNode* child);
AstNode* astnode_continue_new(Token* keyword);
AstNode* astnode_return_new(Token* keyword, AstNode* child);

AstNode* astnode_unop_new(UnopKind kind, Token* op, AstNode* child);
AstNode* astnode_deref_new(AstNode* child, Token* dot, Token* star);
AstNode* astnode_index_new(AstNode* left, AstNode* idx, Token* end);
AstNode* astnode_arith_binop_new(ArithBinopKind kind, Token* op, AstNode* left, AstNode* right);
AstNode* astnode_bool_binop_new(BoolBinopKind kind, Token* op, AstNode* left, AstNode* right);
AstNode* astnode_cmp_binop_new(CmpBinopKind kind, Token* op, AstNode* left, AstNode* right);
AstNode* astnode_assign_new(Token* equal, AstNode* left, Span left_span, AstNode* right);
AstNode* astnode_cast_new(Token* op, AstNode* left, AstNode* right);

AstNode* astnode_import_new(Token* keyword, Token* arg, struct Typespec* mod_ty, char* name);
AstNode* astnode_function_header_new(
    Token* start,
    Token* identifier,
    AstNode** params,
    AstNode* ret_typespec);
AstNode* astnode_function_def_new(Token* export, AstNode* header, AstNode* body);
AstNode* astnode_extern_function_new(Token* extrn, AstNode* header);
AstNode* astnode_variable_decl_new(
    Token* start,
    Token* identifier,
    AstNode* typespec,
    Token* equal,
    AstNode* initializer,
    bool stack,
    bool immutable);
AstNode* astnode_extern_variable_new(
    Token* start,
    Token* identifier,
    AstNode* typespec,
    bool immutable);
AstNode* astnode_param_new(Token* identifier, AstNode* typespec);
AstNode* astnode_exprstmt_new(AstNode* expr);
AstNode* astnode_struct_new(
    Token* keyword,
    Token* identifier,
    AstNode** fields,
    Token* rbrace);

char* astnode_get_name(AstNode* astnode);

#endif
