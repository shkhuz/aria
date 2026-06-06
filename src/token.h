#ifndef TOKEN_H
#define TOKEN_H

#include "core.h"
#include "srcfile.h"

typedef enum {
    TK_KW_CONSTANT,
    TK_KW_MUTABLE,
    TK_KW_FUNCTION,
    TK_KW_STRUCT,
    TK_IDENT,
    TK_STRINGLIT,
    TK_INTEGERLIT,
    TK_EOF,
} TokenKind;

typedef struct Token {
    TokenKind kind;
    Span span;
} Token;

typedef struct {
    char* k;
    TokenKind v;
} StringTokenKindTup;
extern const StringTokenKindTup keywords[];
extern const usize KEYWORDS_LEN;

Token* token_new(TokenKind kind, Span span);
bool   token_lexeme_eqlto(Token* token, const char* string);
bool   token_lexeme_eql(Token* a, Token* b);
char*  token_tostring(Token* token);
char*  tokenkind_tostring(TokenKind kind);

#endif
