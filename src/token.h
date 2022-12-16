#ifndef TOKEN_H
#define TOKEN_H

#include "core.h"
#include "span.h"

typedef enum {
    TOKEN_IDENTIFIER,
    TOKEN_KEYWORD_IMM,
    TOKEN_KEYWORD_MUT,
    TOKEN_KEYWORD_FN,
    TOKEN_KEYWORD_TYPE,
    TOKEN_KEYWORD_STRUCT,
    TOKEN_KEYWORD_IF,
    TOKEN_KEYWORD_ELSE,
    TOKEN_KEYWORD_FOR,
    TOKEN_KEYWORD_RETURN,
    TOKEN_KEYWORD_YIELD,
    TOKEN_STRING,
    TOKEN_INTEGER_LITERAL,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACK,
    TOKEN_RBRACK,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_COLON,
    TOKEN_DOUBLE_COLON,
    TOKEN_SEMICOLON,
    TOKEN_DOT,
    TOKEN_COMMA,
    TOKEN_EQUAL,
    TOKEN_DOUBLE_EQUAL,
    TOKEN_BANG,
    TOKEN_BANG_EQUAL,
    TOKEN_LANGBR,
    TOKEN_LANGBR_EQUAL,
    TOKEN_RANGBR,
    TOKEN_RANGBR_EQUAL,
    TOKEN_AMP,
    TOKEN_DOUBLE_AMP,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_AT,
    TOKEN_DOLLAR,
    TOKEN_FSLASH,
    TOKEN_EOF,
} TokenKind;

typedef struct {
    TokenKind kind;
    Span span;
} Token;

Token* token_new(TokenKind kind, Span span);
bool is_token_lexeme(Token* token, const char* string);
bool can_token_start_expr(Token* token);

#endif
