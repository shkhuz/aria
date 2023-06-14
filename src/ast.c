#include "ast.h"
#include "core.h"
#include "compile.h"

AstNode* astnode_alloc(AstNodeKind kind, Span span) {
    AstNode* astnode = alloc_obj(AstNode);
    astnode->kind = kind;
    astnode->span = span;
    astnode->short_span = span;
    astnode->typespec = NULL;
    return astnode;
}

AstNode* astnode_typespec_func_new(Token* start, AstNode** params, AstNode* ret_typespec) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_TYPESPEC_FUNC,
        span_from_two(start->span, ret_typespec->span));
    astnode->typefunc.params = params;
    astnode->typefunc.ret_typespec = ret_typespec;
    return astnode;
}

AstNode* astnode_typespec_ptr_new(Token* star, bool immutable, AstNode* child) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_TYPESPEC_PTR,
        span_from_two(star->span, child->span));
    astnode->typeptr.immutable = immutable;
    astnode->typeptr.child = child;
    return astnode;
}

AstNode* astnode_typespec_multiptr_new(Token* start, bool immutable, AstNode* child) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_TYPESPEC_MULTIPTR,
        span_from_two(start->span, child->span));
    astnode->typemulptr.immutable = immutable;
    astnode->typemulptr.child = child;
    return astnode;
}

AstNode* astnode_typespec_slice_new(Token* lbrack, bool immutable, AstNode* child) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_TYPESPEC_SLICE,
        span_from_two(lbrack->span, child->span));
    astnode->typeslice.immutable = immutable;
    astnode->typeslice.child = child;
    return astnode;
}

AstNode* astnode_typespec_array_new(Token* lbrack, AstNode* size, AstNode* child) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_TYPESPEC_ARRAY,
        span_from_two(lbrack->span, child->span));
    astnode->typearray.size = size;
    astnode->typearray.child = child;
    return astnode;
}

AstNode* astnode_typespec_tuple_new(Token* start, AstNode** elems, Token* end) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_TYPESPEC_TUPLE,
        span_from_two(start->span, end->span));
    astnode->typetup.elems = elems;
    return astnode;
}

AstNode* astnode_generic_typespec_new(AstNode* left, AstNode** args, Token* end) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_GENERIC_TYPESPEC,
        span_from_two(left->span, end->span));
    astnode->genty.left = left;
    astnode->genty.args = args;
    return astnode;
}

AstNode* astnode_directive_new(
    Token* start,
    Token* callee,
    AstNode** args,
    Token* rparen) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_DIRECTIVE,
        span_from_two(start->span, rparen->span));
    astnode->directive.callee = callee;
    astnode->directive.args = args;
    return astnode;
}

AstNode* astnode_field_new(Token* key, AstNode* value) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_FIELD,
        span_from_two(key->span, value->span));
    astnode->field.key = key;
    astnode->field.value = value;
    return astnode;
}

AstNode* astnode_field_in_literal_new(Token* start, Token* key, AstNode* value) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_FIELD,
        span_from_two(start->span, value->span));
    astnode->field.key = key;
    astnode->field.value = value;
    return astnode;
}

AstNode* astnode_integer_literal_new(Token* token, bigint val) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_INTEGER_LITERAL,
        token->span);
    astnode->intl.token = token;
    astnode->intl.val = val;
    return astnode;
}

AstNode* astnode_array_literal_new(Token* lbrack, AstNode** elems, Token* end) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_ARRAY_LITERAL,
        span_from_two(lbrack->span, end->span));
    astnode->arrayl.elems = elems;
    return astnode;
}

AstNode* astnode_tuple_literal_new(Token* start, AstNode** elems, Token* end) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_TUPLE_LITERAL,
        span_from_two(start->span, end->span));
    astnode->tupl.elems = elems;
    return astnode;
}

AstNode* astnode_aggregate_literal_new(AstNode* typespec, AstNode** fields, Token* end) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_AGGREGATE_LITERAL,
        span_from_two(typespec->span, end->span));
    astnode->aggl.typespec = typespec;
    astnode->aggl.fields = fields;
    return astnode;
}

AstNode* astnode_symbol_new(Token* identifier) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_SYMBOL,
        identifier->span);
    astnode->short_span = identifier->span;
    astnode->sym.identifier = identifier;
    astnode->sym.ref = NULL;
    return astnode;
}

AstNode* astnode_scoped_block_new(
    Token* lbrace,
    AstNode** stmts,
    AstNode* val,
    Token* rbrace)
{
    AstNode* astnode = astnode_alloc(
        ASTNODE_SCOPED_BLOCK,
        span_from_two(lbrace->span, rbrace->span));
    astnode->blk.stmts = stmts;
    astnode->blk.val = val;
    return astnode;
}

AstNode* astnode_if_branch_new(
    Token* keyword,
    IfBranchKind kind,
    AstNode* cond,
    AstNode* body)
{
    AstNode* astnode = astnode_alloc(
        ASTNODE_IF_BRANCH,
        span_from_two(keyword->span, body->span));
    astnode->ifbr.kind = kind;
    astnode->ifbr.cond = cond;
    astnode->ifbr.body = body;
    return astnode;
}

AstNode* astnode_if_new(
    AstNode* ifbr,
    AstNode** elseifbr,
    AstNode* elsebr,
    AstNode* lastbr)
{
    AstNode* astnode = astnode_alloc(
        ASTNODE_IF,
        span_from_two(ifbr->span, lastbr->span));
    astnode->iff.ifbr = ifbr;
    astnode->iff.elseifbr = elseifbr;
    astnode->iff.elsebr = elsebr;
    return astnode;
}

AstNode* astnode_return_new(Token* keyword, AstNode* operand) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_RETURN,
        span_from_two(keyword->span, operand ? operand->span : keyword->span));
    astnode->ret.operand = operand;
    return astnode;
}

AstNode* astnode_function_call_new(AstNode* callee, AstNode** args, Token* end) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_FUNCTION_CALL,
        span_from_two(callee->span, end->span));
    astnode->funcc.callee = callee;
    astnode->funcc.args = args;
    astnode->funcc.rparen = end;
    return astnode;
}

AstNode* astnode_access_new(Token* op, AstNode* left, AstNode* right) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_ACCESS,
        span_from_two(left->span, right->span));
    astnode->short_span = op->span;
    astnode->acc.left = left;
    astnode->acc.right = right;
    return astnode;
}

AstNode* astnode_unop_new(UnopKind kind, Token* op, AstNode* child) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_UNOP,
        span_from_two(op->span, child->span));
    astnode->short_span = op->span;
    astnode->unop.kind = kind;
    astnode->unop.child = child;
    return astnode;
}

AstNode* astnode_deref_new(AstNode* child, Token* dot, Token* star) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_DEREF,
        span_from_two(child->span, star->span));
    astnode->short_span = span_from_two(dot->span, star->span);
    astnode->deref.child = child;
    return astnode;
}

AstNode* astnode_index_new(AstNode* left, AstNode* idx, Token* end) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_INDEX,
        span_from_two(left->span, end->span));
    astnode->idx.left = left;
    astnode->idx.idx = idx;
    return astnode;
}

AstNode* astnode_binop_new(BinopKind kind, Token* op, AstNode* left, AstNode* right) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_BINOP,
        span_from_two(left->span, right->span));
    astnode->short_span = op->span;
    astnode->binop.kind = kind;
    astnode->binop.left = left;
    astnode->binop.right = right;
    return astnode;
}

AstNode* astnode_assign_new(Token* equal, AstNode* left, AstNode* right) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_ASSIGN,
        span_from_two(left->span, right->span));
    astnode->short_span = equal->span;
    astnode->assign.left = left;
    astnode->assign.right = right;
    return astnode;
}

AstNode* astnode_import_new(Token* keyword, Token* arg, struct Typespec* mod_ty, char* name) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_IMPORT,
        span_from_two(keyword->span, arg->span));
    astnode->typespec = mod_ty;
    astnode->import.arg = arg;
    astnode->import.mod_ty = mod_ty;
    astnode->import.name = name;
    return astnode;
}

AstNode* astnode_function_header_new(
    Token* start,
    Token* identifier,
    AstNode** params,
    AstNode* ret_typespec)
{
    AstNode* astnode = astnode_alloc(
        ASTNODE_FUNCTION_HEADER,
        span_from_two(start->span, ret_typespec->span));
    astnode->funch.identifier = identifier;
    astnode->funch.name = token_tostring(identifier);
    astnode->funch.params = params;
    astnode->funch.ret_typespec = ret_typespec;
    return astnode;
}

AstNode* astnode_function_def_new(AstNode* header, AstNode* body, bool global) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_FUNCTION_DEF,
        span_from_two(header->span, body->span));
    astnode->funcdef.header = header;
    astnode->funcdef.body = body;
    astnode->funcdef.global = global;
    return astnode;
}

AstNode* astnode_variable_decl_new(
    Token* start,
    Token* identifier,
    AstNode* typespec,
    Token* equal,
    AstNode* initializer,
    bool stack,
    bool immutable)
{
    AstNode* astnode = astnode_alloc(
        ASTNODE_VARIABLE_DECL,
        span_from_two(start->span, initializer->span));
    astnode->vard.identifier = identifier;
    astnode->vard.name = token_tostring(identifier);
    astnode->vard.typespec = typespec;
    astnode->vard.equal = equal;
    astnode->vard.initializer = initializer;
    astnode->vard.immutable = immutable;
    astnode->vard.stack = stack;
    return astnode;
}

AstNode* astnode_param_new(Token* identifier, AstNode* typespec) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_PARAM_DECL,
        span_from_two(identifier->span, typespec->span));
    astnode->paramd.identifier = identifier;
    astnode->paramd.name = token_tostring(identifier);
    astnode->paramd.typespec = typespec;
    return astnode;
}

AstNode* astnode_exprstmt_new(AstNode* expr) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_EXPRSTMT,
        expr->span);
    astnode->exprstmt = expr;
    return astnode;
}

AstNode* astnode_struct_new(
    Token* keyword,
    Token* identifier,
    AstNode** fields,
    Token* rbrace) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_STRUCT,
        span_from_two(keyword->span, rbrace->span));
    astnode->strct.identifier = identifier;
    astnode->strct.name = token_tostring(identifier);
    astnode->strct.fields = fields;
    return astnode;
}

char* astnode_get_name(AstNode* astnode) {
    switch (astnode->kind) {
        case ASTNODE_FUNCTION_DEF: {
            return astnode_get_name(astnode->funcdef.header);
        } break;

        case ASTNODE_FUNCTION_HEADER: {
            return astnode->funch.name;
        } break;

        case ASTNODE_STRUCT: {
            return astnode->strct.name;
        } break;

        case ASTNODE_VARIABLE_DECL: {
            return astnode->vard.name;
        } break;

        case ASTNODE_IMPORT: {
            return astnode->import.name;
        } break;

        default: {
            assert(0);
        } break;
    }
}
