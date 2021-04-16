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
	MSG_TY_ROOT, 
	MSG_TY_ERR,
} MsgType;

void msg_user(MsgType ty, u32 code, char* fmt, ...);
void terminate_compilation();

#define ERROR_CANNOT_READ_SOURCE_FILE 1, "cannot read source file '%s'"

///// LEXER /////
typedef struct {
	SrcFile* srcfile;
	char* start, *current;
	uint line;
} Lexer;

void lexer_init(Lexer* self, SrcFile* srcfile);
void lexer_lex(Lexer* self);

///// MISC /////
extern char* executable_path_from_argv;
extern char* keywords[KEYWORDS_LEN];

#endif	/* __ARIA_H */
