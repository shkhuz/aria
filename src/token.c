#include "token.h"
#include "compile.h"

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
        || token->kind == TOKEN_AT
        || token->kind == TOKEN_LBRACE
        || token->kind == TOKEN_KEYWORD_IF
        || token->kind == TOKEN_KEYWORD_RETURN
        || token->kind == TOKEN_INTEGER_LITERAL) {
        return true;
    }
    return false;
}

char* tokenkind_to_string(TokenKind kind) {
    switch (kind) {
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_LBRACK: return "[";
        case TOKEN_RBRACK: return "]";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_LANGBR: return "<";
        case TOKEN_RANGBR: return ">";
        default: abort();

    }
}
