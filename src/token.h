#ifndef TOKEN_H
#define TOKEN_H

#include "core.h"
#include "span.h"

typedef enum {
    TOKEN_KEYWORD_IMM,
    TOKEN_KEYWORD_MUT,
    TOKEN_KEYWORD_PUB,
    TOKEN_KEYWORD_FN,
    TOKEN_KEYWORD_TYPE,
    TOKEN_KEYWORD_STRUCT,
    TOKEN_KEYWORD_UNION,
    TOKEN_KEYWORD_ENUM,
    TOKEN_KEYWORD_TRAIT,
    TOKEN_KEYWORD_IMPL,
    TOKEN_KEYWORD_USE,
    TOKEN_KEYWORD_WHERE,
    TOKEN_KEYWORD_IMPORT,
    TOKEN_KEYWORD_EXPORT,
    TOKEN_KEYWORD_EXTERN,
    TOKEN_KEYWORD_IF,
    TOKEN_KEYWORD_ELSE,
    TOKEN_KEYWORD_WHILE,
    TOKEN_KEYWORD_FOR,
    TOKEN_KEYWORD_BREAK,
    TOKEN_KEYWORD_CONTINUE,
    TOKEN_KEYWORD_RETURN,
    TOKEN_KEYWORD_YIELD,
    TOKEN_KEYWORD_AS,
    TOKEN_KEYWORD_AND,
    TOKEN_KEYWORD_OR,
    TOKEN_KEYWORD_UNDERSCORE,
    TOKEN_KEYWORD_TRUE,
    TOKEN_KEYWORD_FALSE,

    TOKEN_IDENTIFIER,
    TOKEN_STRING_LITERAL,
    TOKEN_CHAR_LITERAL,
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
    TOKEN_PERC,
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

    union {
        char c;
        char* str;
    };
} Token;

Token* token_new(TokenKind kind, Span span);
bool is_token_lexeme(Token* token, const char* string);
bool are_token_lexemes_equal(Token* a, Token* b);
bool can_token_start_typespec(Token* token);
bool can_token_start_expr(Token* token);
char* token_tostring(Token* token);
char* tokenkind_to_string(TokenKind kind);

#endif
