#include "misc.h"
#include "buf.h"

StringKwKind* keywords = NULL;

void init_types() {
    bufpush(keywords, (StringKwKind){ "imm", TOKEN_KEYWORD_IMM });
    bufpush(keywords, (StringKwKind){ "mut", TOKEN_KEYWORD_MUT });
    bufpush(keywords, (StringKwKind){ "fn", TOKEN_KEYWORD_FN });
    bufpush(keywords, (StringKwKind){ "type", TOKEN_KEYWORD_TYPE });
    bufpush(keywords, (StringKwKind){ "struct", TOKEN_KEYWORD_STRUCT });
    bufpush(keywords, (StringKwKind){ "if", TOKEN_KEYWORD_IF });
    bufpush(keywords, (StringKwKind){ "else", TOKEN_KEYWORD_ELSE });
    bufpush(keywords, (StringKwKind){ "for", TOKEN_KEYWORD_FOR });
    bufpush(keywords, (StringKwKind){ "return", TOKEN_KEYWORD_RETURN });
    bufpush(keywords, (StringKwKind){ "yield", TOKEN_KEYWORD_YIELD });
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
