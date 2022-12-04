#include "token.h"
#include "misc.h"

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

bool can_token_start_expr(Token* token) {
    if (token->kind == TOKEN_IDENTIFIER
        || token->kind == TOKEN_LBRACE
        || token->kind == TOKEN_KEYWORD_IF
        || token->kind == TOKEN_KEYWORD_RETURN
        || token->kind == TOKEN_INTEGER_LITERAL) {
        return true;
    }
    return false;
}
