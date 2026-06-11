#ifndef TOKEN_H
#define TOKEN_H

#include "core.h"
#include "srcfile.h"

#define FOREACH_TOKENKIND(WRAP)\
    WRAP(TK_KW_IMM)\
    WRAP(TK_KW_MUT)\
    WRAP(TK_KW_FN)\
    WRAP(TK_KW_STRUCT)\
    WRAP(TK_IDENT)\
    WRAP(TK_STRLIT)\
    WRAP(TK_INTLIT)\
    WRAP(TK_SEMICOLON)\
    WRAP(TK_EOF)\
    WRAP(TK_LEN)

typedef enum {
    FOREACH_TOKENKIND(ENUM_GEN)
} TokenKind;

extern const char* tokenkind_strs[];

typedef struct Token {
    TokenKind kind;
    Span span;
} Token;

typedef struct {
    char* k;
    TokenKind v;
} StringTokenKindTup;
extern const StringTokenKindTup keywords[];
extern const int KEYWORDS_LEN;

Token* token_new(TokenKind kind, Span span);
bool   token_lexeme_eqlto(Token* token, const char* string);
bool   token_lexeme_eql(Token* a, Token* b);
char*  token_tostring(Token* token);
char*  tokenkind_tostring(TokenKind kind);

#endif
