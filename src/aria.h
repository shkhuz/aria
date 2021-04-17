#ifndef __ARIA_H
#define __ARIA_H

#include <aria_core.h>

///// TYPES /////
typedef enum {
	TT_IDENT,
	TT_KEYWORD,
	TT_LPAREN,
	TT_RPAREN,
	TT_LBRACE,
	TT_RBRACE,
	TT_NONE,
} TokenType;

typedef struct {
	TokenType ty;
	char* start, *end;
} Token;

#define KEYWORD_FN "fn"
#define KEYWORD_UNDERSCORE "_"
#define KEYWORDS_LEN 2

typedef struct {
	File* contents;
	Token** tokens;
} SrcFile;

Token* token_alloc(TokenType ty, char* start, char* end);

///// COMPILE ERROR/WARNING /////
typedef enum {
	MSG_TY_ROOT_ERR, 
	MSG_TY_ERR,
} MsgType;

void vmsg_user(MsgType ty, SrcFile* srcfile, u64 line, u64 column, u64 char_count, u32 code, char* fmt, va_list ap);
void msg_user(MsgType ty, SrcFile* srcfile, u64 line, u64 column, u64 char_count, u32 code, char* fmt, ...);
void terminate_compilation();

#define ERROR_CANNOT_READ_SOURCE_FILE 					1, "cannot read source file '%s'"
#define ERROR_ABORTING_DUE_TO_PREV_ERRORS 				2, "aborting due to previous error(s)"
#define ERROR_INVALID_CHAR 								3, "invalid character"
#define ERROR_NO_SOURCE_FILES_PASSED_IN_CMD_ARGS 		4, "one or more input files needed"

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

///// MISC /////
extern char* executable_path_from_argv;
extern char* keywords[KEYWORDS_LEN];

#endif	/* __ARIA_H */
