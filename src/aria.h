#ifndef __ARIA_H
#define __ARIA_H

#include <aria_core.h>

///// TYPES /////
typedef struct Token Token; 

typedef struct {
	File* contents;
	struct Token** tokens;
} SrcFile;

typedef enum {
	TT_IDENT,
	TT_KEYWORD,
	TT_LPAREN,
	TT_RPAREN,
	TT_LBRACE,
	TT_RBRACE,
	TT_SEMICOLON,
	TT_PLUS,
	TT_MINUS,
	TT_EOF,
	TT_NONE,
} TokenType;

struct Token {
	TokenType ty;
	char* lexeme, *start, *end;
	SrcFile* srcfile;
	u64 line, column, char_count;
};

Token* token_alloc(
		TokenType ty, 
		char* lexeme, 
		char* start, 
		char* end, 
		SrcFile* srcfile, 
		u64 line, 
		u64 column, 
		u64 char_count);

typedef enum {
	ET_BINARY,
	ET_IDENT,
} ExprType;

typedef struct Expr Expr;

struct Expr {
	ExprType ty;
	union {
		Token* ident;	

		struct {
			Expr* left, *right;
			Token* op;
		} binary;
	};
};

typedef enum {
	ST_FUNCTION,
	ST_EXPR,
	ST_NONE,
} StmtType;

typedef struct {
	StmtType ty;
	union {
		Expr* expr;
	};
} Stmt;

Stmt* stmt_function_alloc();

#define KEYWORD_FN "fn"
#define KEYWORD_UNDERSCORE "_"
#define KEYWORDS_LEN 2

///// COMPILE ERROR/WARNING /////
typedef enum {
	MSG_TY_ROOT_ERR, 
	MSG_TY_ERR,
} MsgType;

void vmsg_user(
		MsgType ty, 
		SrcFile* srcfile, 
		u64 line, 
		u64 column, 
		u64 char_count, 
		u32 code, 
		char* fmt, 
		va_list ap);

void msg_user(
		MsgType ty, 
		SrcFile* srcfile, 
		u64 line, 
		u64 column, 
		u64 char_count, 
		u32 code, 
		char* fmt, 
		...);

void vmsg_user_token(
		MsgType ty,
		Token* token,
		u32 code,
		char* fmt, 
		va_list ap);

void msg_user_token(
		MsgType ty,
		Token* token,
		u32 code,
		char* fmt, 
		...);

void terminate_compilation();

#define ERROR_CANNOT_READ_SOURCE_FILE 					1, "cannot read source file '%s'"
#define ERROR_ABORTING_DUE_TO_PREV_ERRORS 				2, "aborting due to previous error(s)"
#define ERROR_INVALID_CHAR 								3, "invalid character"
#define ERROR_NO_SOURCE_FILES_PASSED_IN_CMD_ARGS 		4, "one or more input files needed"
#define ERROR_EXPECT_SEMICOLON							5, "expect semicolon"
#define ERROR_UNEXPECTED_TOKEN							6, "unexpected token"

///// LEXER /////
typedef struct {
	SrcFile* srcfile;
	char* start, *current;
	u64 line;
	char* last_newline;
	bool error;
} Lexer;

void lexer_init(Lexer* self, SrcFile* srcfile);
void lexer_lex(Lexer* self);

///// PARSER /////
typedef struct {
	SrcFile* srcfile;
	u64 token_idx;
	bool error;
	u64 error_count;
} Parser;

void parser_init(Parser* self, SrcFile* srcfile);
void parser_parse(Parser* self);

///// MISC /////
extern char* executable_path_from_argv;
extern char* keywords[KEYWORDS_LEN];

#endif	/* __ARIA_H */
