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

bool are_token_lexemes_equal(Token* a, Token* b) {
    if ((a->span.end-a->span.start) != (b->span.end-b->span.start)) return false;
    for (usize ia = a->span.start, ib = b->span.start;
         ia < a->span.end && ib < b->span.end;
         ia++, ib++)
    {
        if (a->span.srcfile->handle.contents[ia] != b->span.srcfile->handle.contents[ib]) {
            return false;
        }
    }
    return true;
}

bool can_token_start_typespec(Token* token) {
    if (token->kind == TOKEN_IDENTIFIER
        || token->kind == TOKEN_LBRACK
        || token->kind == TOKEN_STAR
        || token->kind == TOKEN_KEYWORD_FN) {
        return true;
    }
    return false;
}

bool can_token_start_expr(Token* token) {
    if (token->kind == TOKEN_IDENTIFIER
        || token->kind == TOKEN_LBRACE
        || token->kind == TOKEN_KEYWORD_IF
        || token->kind == TOKEN_KEYWORD_WHILE
        || token->kind == TOKEN_KEYWORD_FOR
        || token->kind == TOKEN_KEYWORD_BREAK
        || token->kind == TOKEN_KEYWORD_CONTINUE
        || token->kind == TOKEN_KEYWORD_RETURN
        || token->kind == TOKEN_INTEGER_LITERAL
        || token->kind == TOKEN_STRING_LITERAL
        || token->kind == TOKEN_CHAR_LITERAL
        || token->kind == TOKEN_DOT
        || token->kind == TOKEN_LBRACK
        || token->kind == TOKEN_LPAREN
        || token->kind == TOKEN_MINUS
        || token->kind == TOKEN_BANG
        || token->kind == TOKEN_AMP
        || token->kind == TOKEN_KEYWORD_UNDERSCORE) {
        return true;
    }
    return false;
}

char* token_tostring(Token* token) {
    return span_tostring(token->span);
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
