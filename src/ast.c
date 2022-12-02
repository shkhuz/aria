#include "ast.h"
#include "core.h"

AstNode* astnode_alloc(AstNodeKind kind, Span span) {
    AstNode* astnode = alloc_obj(AstNode);
    astnode->kind = kind;
    astnode->span = span;
    return astnode;
}

AstNode* astnode_typespec_ptr_new(Token* star, AstNode* child) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_TYPESPEC_PTR,
        span_from_two(star->span, child->span));
    astnode->typeptr.child = child;
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

AstNode* astnode_symbol_new(Token* identifier, bool is_typespec) {
    AstNode* astnode = astnode_alloc(
        is_typespec ? ASTNODE_TYPESPEC_SYMBOL : ASTNODE_SYMBOL, 
        identifier->span);
    astnode->sym.identifier = identifier;
    return astnode;
}

AstNode* astnode_scoped_block_new(Token* lbrace, AstNode** stmts, Token* rbrace) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_SCOPED_BLOCK, 
        span_from_two(lbrace->span, rbrace->span));
    astnode->blk.stmts = stmts;
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
    astnode->funch.params = params;
    astnode->funch.ret_typespec = ret_typespec;
    return astnode;
}

AstNode* astnode_function_def_new(AstNode* header, AstNode* body) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_FUNCTION_DEF,
        span_from_two(header->span, body->span));
    astnode->funcd.header = header;
    astnode->funcd.body = body;
    return astnode;
}

AstNode* astnode_variable_decl_new(
    Token* start,
    bool immutable,
    Token* identifier,
    AstNode* typespec,
    AstNode* initializer,
    Token* end)
{
    AstNode* astnode = astnode_alloc(
        ASTNODE_VARIABLE_DECL,
        span_from_two(start->span, end->span));
    astnode->vard.immutable = immutable;
    astnode->vard.identifier = identifier;
    astnode->vard.typespec = typespec;
    astnode->vard.initializer = initializer;
    return astnode;
}

AstNode* astnode_param_new(Token* identifier, AstNode* typespec) {
    AstNode* astnode = astnode_alloc(
        ASTNODE_PARAM_DECL,
        span_from_two(identifier->span, typespec->span));
    astnode->paramd.identifier = identifier;
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
