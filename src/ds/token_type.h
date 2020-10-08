#ifndef _TOKEN_TYPE_H
#define _TOKEN_TYPE_H

typedef enum {
	T_IDENTIFIER = 0,
	T_KEYWORD,
    T_INTEGER_U8,
    T_INTEGER_U16,
    T_INTEGER_U32,
    T_INTEGER_U64,
    T_INTEGER_I8,
    T_INTEGER_I16,
    T_INTEGER_I32,
    T_INTEGER_I64,
    T_FLOAT32,
    T_STRING,
    T_L_BRACE,
    T_R_BRACE,
    T_L_PAREN,
    T_R_PAREN,
    T_PLUS,
    T_MINUS,
    T_STAR,
    T_AMPERSAND,
    T_SEMICOLON,
    T_COLON,
    T_COMMA,
    T_EQUAL,
    T_EOF,
} TokenType;

#endif /* _TOKEN_TYPE_H */
