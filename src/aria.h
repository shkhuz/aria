#ifndef __ARIA_H
#define __ARIA_H

#include <aria_core.h>

///// TYPES /////
typedef enum {
	TT_IDENT,
	TT_KEYWORD,
	TT_NONE,
} TokenType;

typedef struct {
	TokenType ty;
	char* start, *end;
} Token;

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

#endif	/* __ARIA_H */
