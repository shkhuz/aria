typedef struct SrcFile SrcFile;

char* keywords[] = {
	"module",
	"struct",
	"proc",
	"let",
	"mut",
	"const",
};

char* directives[] = {
	"@import",
};

typedef enum {
	TK_IDENT,
	TK_KEYWORD,
	TK_DIRECTIVE,
	TK_STRING,
	TK_LPAREN,
	TK_RPAREN,
	TK_LBRACE,
	TK_RBRACE,
	TK_SEMICOLON,
	TK_COLON,
	TK_DOUBLE_COLON,
	TK_COMMA,
	TK_PLUS,
	TK_MINUS,
	TK_STAR,
	TK_FSLASH,
	TK_EQUAL,
	TK_AMPERSAND,
	TK_EOF,
	TK_NONE,
} TokenKind;

typedef struct {
	TokenKind kind;
	char* lexeme, *start, *end;
	SrcFile* srcfile;
	u64 line, column, char_count;
} Token;

Token* token_alloc(
		TokenKind kind,
		char* lexeme, 
		char* start,
		char* end,
		SrcFile* srcfile,
		u64 line,
		u64 column,
		u64 char_count) {
	alloc_with_type(token, Token);
	*token = (Token) {
		kind,
		lexeme,
		start,
		end,
		srcfile,
		line,
		column,
		char_count
	};
	return token;
}

typedef enum {
	NK_FUNCTION,
} NodeKind;

typedef struct {
	NodeKind kind;
	Token* head;
	Token* tail;
} Node;

struct SrcFile {
	File* contents;
	Token** tokens;
	Node** nodes;
};

/* typedef struct Scope { */
/* 	struct Scope* parent; */
/* 	Stmt** sym_tbl; */
/* } Scope; */

/* typedef struct { */
/* 	SrcFile* srcfile; */
/* 	Scope* global_scope; */
/* 	Scope* current_scope; */
/* 	bool error; */
/* } Resolver; */

/* void resolver_init(Resolver* self, SrcFile* srcfile); */
/* void resolver_resolve(Resolver* self); */

/* typedef struct { */
/* 	SrcFile* srcfile; */
/* 	bool error; */
/* 	u64 error_count; */
/* } Checker; */

/* void checker_init(Checker* self, SrcFile* srcfile); */
/* void checker_check(Checker* self); */
