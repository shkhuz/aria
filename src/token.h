#ifndef TOKEN_H
#define TOKEN_H

#include "types.h"
#include "core.h"
#include "srcfile.h"

#define FOREACH_TOKENKIND(WRAP)\
    WRAP(TK_COLON)\
    WRAP(TK_COMMA)\
    WRAP(TK_EOF)\
    WRAP(TK_EQUAL)\
    WRAP(TK_IDENT)\
    WRAP(TK_INTLIT)\
    WRAP(TK_KW_COMP)\
    WRAP(TK_KW_FUN)\
    WRAP(TK_KW_IMM)\
    WRAP(TK_KW_MUT)\
    WRAP(TK_KW_STRUCT)\
    WRAP(TK_KW_YIELD)\
    WRAP(TK_LBRACE)\
    WRAP(TK_LPAREN)\
    WRAP(TK_RBRACE)\
    WRAP(TK_RPAREN)\
    WRAP(TK_SEMICOLON)\
    WRAP(TK_STRLIT)\
    WRAP(TK_LEN)

typedef enum {
    FOREACH_TOKENKIND(ENUM_GEN)
} TokenKind;

extern const char* tokenkind_strs[];

struct Token {
    TokenKind kind;
    Span span;
    int extra;
};

typedef struct {
    const char* str;
    int len;
} StrlitData;
extern StrlitData* token_strlit_data;

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
