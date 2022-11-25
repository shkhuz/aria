#include "types.h"
#include "buf.h"

StringKwKind* keywords = NULL;

TypePlaceholders type_placeholders;

void init_types() {
    bufpush(keywords, (StringKwKind){ "imm", TOKEN_KIND_KEYWORD_IMM });
    bufpush(keywords, (StringKwKind){ "mut", TOKEN_KIND_KEYWORD_MUT });
    bufpush(keywords, (StringKwKind){ "fn", TOKEN_KIND_KEYWORD_FN });
    bufpush(keywords, (StringKwKind){ "struct", TOKEN_KIND_KEYWORD_STRUCT });
    bufpush(keywords, (StringKwKind){ "if", TOKEN_KIND_KEYWORD_IF });
    bufpush(keywords, (StringKwKind){ "else", TOKEN_KIND_KEYWORD_ELSE });
    bufpush(keywords, (StringKwKind){ "for", TOKEN_KIND_KEYWORD_FOR });
    bufpush(keywords, (StringKwKind){ "return", TOKEN_KIND_KEYWORD_RETURN });

    type_placeholders.u8_type = type_primitive_init(TYPE_PRIM_U8);
    type_placeholders.u16_type = type_primitive_init(TYPE_PRIM_U16);
    type_placeholders.u32_type = type_primitive_init(TYPE_PRIM_U32);
    type_placeholders.u64_type = type_primitive_init(TYPE_PRIM_U64);
    type_placeholders.i8_type = type_primitive_init(TYPE_PRIM_I8);
    type_placeholders.i16_type = type_primitive_init(TYPE_PRIM_I16);
    type_placeholders.i32_type = type_primitive_init(TYPE_PRIM_I32);
    type_placeholders.i64_type = type_primitive_init(TYPE_PRIM_I64);
    type_placeholders.void_type = type_primitive_init(TYPE_PRIM_VOID);
}

Token* token_new(TokenKind kind, Span span) {
    Token* token = alloc_obj(Token);
    token->kind = kind;
    token->span = span;
    return token;
}

bool is_token_lexeme(Token* token, const char* string) {
    return slice_eql_to_str(
        &token->span.srcfile->handle.contents[token->span.start],
        token->span.end - token->span.start,
        string);
}

Type type_primitive_init(TypePrimitiveKind kind) {
    Type type;
    type.kind = TYPE_KIND_PRIMITIVE;
    type.prim.kind = kind;
    return type;
}

Type type_custom_init(Token* name) {
    Type type;
    type.kind = TYPE_KIND_CUSTOM;
    type.custom.name = name;
    return type;
}

AstNode* astnode_type_new(Type type, Span span) {
    AstNode* astnode = alloc_obj(AstNode);
    astnode->span = span;
    astnode->kind = ASTNODE_KIND_TYPE;
    astnode->type.type = type;
    return astnode;
}

AstNode* astnode_symbol_new(Token* identifier) {
    AstNode* astnode = alloc_obj(AstNode);
    astnode->span = identifier->span;
    astnode->kind = ASTNODE_KIND_SYMBOL;
    astnode->sym.identifier = identifier;
    return astnode;
}

AstNode* astnode_scoped_block_new(
    Token* lbrace,
    AstNode** stmts,
    AstNode* val,
    Token* rbrace)
{
    AstNode* astnode = alloc_obj(AstNode);
    astnode->span = span_from_two(lbrace->span, rbrace->span);
    astnode->kind = ASTNODE_KIND_SCOPED_BLOCK;
    astnode->blk.stmts = stmts;
    astnode->blk.val = val;
    return astnode;
}

AstNode* astnode_if_branch_new(Token* keyword, AstNode* cond, AstNode* body) {
    AstNode* astnode = alloc_obj(AstNode);
    astnode->span = span_from_two(keyword->span, body->span);
    astnode->kind = ASTNODE_KIND_IF_BRANCH;
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
    AstNode* astnode = alloc_obj(AstNode);
    astnode->span = span_from_two(ifbr->span, lastbr->span);
    astnode->kind = ASTNODE_KIND_IF;
    astnode->iff.ifbr = ifbr;
    astnode->iff.elseifbr = elseifbr;
    astnode->iff.elsebr = elsebr;
    return astnode;       
}

AstNode* astnode_return_new(Token* keyword, AstNode* operand) {
    AstNode* astnode = alloc_obj(AstNode);
    astnode->span = span_from_two(keyword->span, operand ? operand->span : keyword->span);
    astnode->kind = ASTNODE_KIND_RETURN;
    astnode->ret.operand = operand;
    return astnode;
}

AstNode* astnode_function_call_new(AstNode* callee, AstNode** args, Token* end) {
    AstNode* astnode = alloc_obj(AstNode);
    astnode->span = span_from_two(callee->span, end->span);
    astnode->kind = ASTNODE_KIND_FUNCTION_CALL;
    astnode->funcc.callee = callee;
    astnode->funcc.args = args;
    return astnode;
}

AstNode* astnode_function_header_new(
    Token* start,
    Token* identifier,
    AstNode** params,
    AstNode* ret_type)
{
    AstNode* astnode = alloc_obj(AstNode);
    astnode->span = span_from_two(start->span, ret_type->span);
    astnode->kind = ASTNODE_KIND_FUNCTION_HEADER;
    astnode->funch.identifier = identifier;
    astnode->funch.params = params;
    astnode->funch.ret_type = ret_type;
    return astnode;
}

AstNode* astnode_function_def_new(AstNode* header, AstNode* body) {
    AstNode* astnode = alloc_obj(AstNode);
    astnode->span = span_from_two(header->span, body->span);
    astnode->kind = ASTNODE_KIND_FUNCTION_DEF;
    astnode->funcd.header = header;
    astnode->funcd.body = body;
    return astnode;
}

AstNode* astnode_variable_decl_new(
    Token* start,
    bool immutable,
    Token* identifier,
    AstNode* type,
    AstNode* initializer,
    Token* end)
{
    AstNode* astnode = alloc_obj(AstNode);
    astnode->span = span_from_two(start->span, end->span);
    astnode->kind = ASTNODE_KIND_VARIABLE_DECL;
    astnode->vard.immutable = immutable;
    astnode->vard.identifier = identifier;
    astnode->vard.type = type;
    astnode->vard.initializer = initializer;
    return astnode;
}

AstNode* astnode_param_new(Token* identifier, AstNode* type) {
    AstNode* astnode = alloc_obj(AstNode);
    astnode->span = span_from_two(identifier->span, type->span);
    astnode->kind = ASTNODE_KIND_PARAM_DECL;
    astnode->paramd.identifier = identifier;
    astnode->paramd.type = type;
    return astnode;
}
