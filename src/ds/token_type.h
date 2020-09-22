#ifndef _TOKEN_TYPE_H
#define _TOKEN_TYPE_H

typedef enum {
	T_IDENTIFIER = 0,
	T_KEYWORD,
    T_INTEGER,
    T_L_BRACE,
    T_R_BRACE,
    T_L_PAREN,
    T_R_PAREN,
    T_SEMICOLON,
} TokenType;

#endif /* _TOKEN_TYPE_H */
