#ifndef _TOKEN_TYPE_H
#define _TOKEN_TYPE_H

typedef enum {
	T_IDENTIFIER = 0,
	T_KEYWORD,
    T_INTEGER,
    T_FLOAT32,
    T_L_BRACE,
    T_R_BRACE,
    T_L_PAREN,
    T_R_PAREN,
    T_STAR,
    T_SEMICOLON,
    T_COLON,
    T_COMMA,
    T_EOF,
} TokenType;

#endif /* _TOKEN_TYPE_H */
