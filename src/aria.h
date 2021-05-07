#ifndef __ARIA_H
#define __ARIA_H

#include <aria_core.h>

///// TYPES /////
typedef struct SrcFile SrcFile; 
typedef struct DataType DataType;
typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef enum {
	TT_IDENT,
	TT_KEYWORD,
	TT_STRING,
	TT_DIRECTIVE,	// `#...`
	TT_LPAREN,
	TT_RPAREN,
	TT_LBRACE,
	TT_RBRACE,
	TT_SEMICOLON,
	TT_COLON,
	TT_DOUBLE_COLON,
	TT_COMMA,
	TT_PLUS,
	TT_MINUS,
	TT_STAR,
	TT_FSLASH,
	TT_EQUAL,
	TT_AMPERSAND,
	TT_EOF,
	TT_NONE,
} TokenType;

typedef struct {
	TokenType ty;
	char* lexeme, *start, *end;
	SrcFile* srcfile;
	u64 line, column, char_count;
} Token;

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
	DT_NAMED,
	DT_STRUCT,
} DataTypeType;

typedef struct {
	Token* ident;
	DataType* dt;
	Expr* initializer;
} Variable;

typedef struct {
	bool is_const;
} Pointer;

// `::`
typedef struct {
	Token** accessors;
	bool from_global_scope;
} StaticAccessor;

struct DataType {
	DataTypeType ty;
	union {
		struct {
			Pointer* pointers;
			StaticAccessor static_accessor;
			Token* ident;
			Stmt* ref;
		} named;

		// TODO: make it so that we can
		// add a pointer `*` after an anonymous struct
		// type.
		struct {
			Token* struct_keyword;
			Token* ident;
			Stmt** fields;
		} struct_;
	};
};

typedef enum {
	ET_BINARY_ADD,
	ET_BINARY_SUBTRACT,
	ET_BINARY_MULTIPLY,
	ET_BINARY_DIVIDE,
	ET_UNARY_DEREF,
	ET_UNARY_REF,
	ET_IDENT,
	ET_STRING,
	ET_BLOCK,
	ET_FUNCTION_CALL,
	ET_ASSIGN,
	ET_NONE,
} ExprType;

struct Expr {
	ExprType ty;
	Token* head;
	union {
		struct {
			StaticAccessor static_accessor;
			Token* ident;	
			Stmt* ref;
		} ident;

		struct {
			Token* string;
		} string;

		struct {
			Stmt** stmts;
			Expr* value;
		} block;

		struct {
			Expr* left;
			Expr** args;
		} function_call;

		struct {
			Token* op;
			Expr* right;
		} unary;

		struct {
			Expr* left, *right;
			Token* op;
		} binary;

		struct {
			Expr* left, *right;
			Token* op;
		} assign;
	};
};

typedef enum {
	ST_IMPORTED_NAMESPACE,
	ST_NAMESPACE,
	ST_STRUCT,
	ST_FUNCTION,
	ST_FUNCTION_PROTOTYPE,
	ST_VARIABLE,
	ST_EXPR,
	ST_NONE,
} StmtType;

typedef struct {
	Token* fn_keyword;
	Token* ident;
	Stmt** params;
	DataType* return_data_type;
} FunctionHeader;

struct Stmt {
	StmtType ty;
	Token* ident;
	bool in_function;
	union {
		struct {
			char* ident;
			char* fpath;
			char* fpath_rel_current_file;
			SrcFile* srcfile;
		} imported_namespace;

		struct {
			Token* namespace_keyword;

			// A `Block` isn't used because namespaces don't
			// return anything, and blocks have value.
			Stmt** stmts;
		} namespace_;

		struct {
			FunctionHeader* header;
			Expr* body;
		} function;

		struct {
			Variable* variable;
			bool is_mut;
		} variable;

		FunctionHeader* function_prototype;
		Expr* expr;
		DataType* struct_;
	};
};

struct SrcFile {
	File* contents;
	Token** tokens;
	Stmt** stmts;
};

///// COMPILE ERROR/WARNING /////
typedef enum {
	MSG_TY_ROOT_ERR, 
	MSG_TY_ERR,
	MSG_TY_NOTE,
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

#define ERROR_CANNOT_READ_SOURCE_FILE 					1, "cannot read source file `%s`"	// TODO: more specific kind of error
#define ERROR_ABORTING_DUE_TO_PREV_ERRORS 				2, "aborting due to previous error(s)"
#define ERROR_INVALID_CHAR 								3, "invalid character"
#define ERROR_NO_SOURCE_FILES_PASSED_IN_CMD_ARGS 		4, "one or more input files needed"
#define ERROR_UNEXPECTED_TOKEN							5, "unexpected token"
#define ERROR_EXPECT_SEMICOLON							6, "expect semicolon"
#define ERROR_EXPECT_IDENT								7, "expect identifier"
#define ERROR_EXPECT_LBRACE								8, "expect `{`"
#define ERROR_EXPECT_RBRACE								9, "expect `}`"
#define ERROR_EXPECT_COLON								10, "expect colon"
#define ERROR_EXPECT_COMMA								11, "expect comma"
#define ERROR_EXPECT_DATA_TYPE							12, "expect data type"
#define ERROR_EXPECT_INITIALIZER_IF_NO_TYPE_SPECIFIED	13, "expect initializer when no type specified"
#define ERROR_EXPECT_LPAREN								14, "expect `(`"
#define ERROR_EXPECT_RPAREN								15, "expect `)`"
#define ERROR_UNTERMINATED_STRING						16, "unterminated string literal"
#define ERROR_UNDEFINED_DIRECTIVE						17, "undefined directive"
#define ERROR_IMPORT_DIRECTIVE_MUST_BE_STRING_LITERAL	18, "import directive must be a compile-time string literal"
#define ERROR_SOURCE_FILE_IS_DIRECTORY					19, "`%s` is a directory"
#define ERROR_IMPORT_DIRECTIVE_IS_EMPTY					20, "empty file name to `#import`"
#define ERROR_INVALID_TOP_LEVEL_TOKEN					21, "invalid top-level token"
#define ERROR_INVALID_NAMESPACE_LEVEL_TOKEN				24, "invalid token in namespace-level scope"
#define ERROR_REDECLARATION_OF_SYMBOL					22, "redeclaration of symbol `%s`"
#define ERROR_INVALID_CHAR_AFTER_DIRECTIVE				23, "invalid character after `#`"
#define ERROR_UNDECLARED_SYMBOL 						25, "undeclared symbol `%s`"
#define ERROR_NAMESPACE_NOT_FOUND						26, "namespace `%s` not found"
#define ERROR_IS_NOT_A_VALID_TYPE						27, "`%s` is not a valid type"
#define ERROR_IS_A										28, "`%s` is a %s"
#define ERROR_CANNOT_CONVERT_TO							29, "cannot convert `%s` to `%s`"
#define ERROR_INVALID_LVALUE							30, "invalid l-value"
#define ERROR_EXPECTED_TYPE_GOT_TYPE					31, "expected type `%s`, got type `%s`"
#define ERROR_CANNOT_ASSIGN_TO							32, "cannot assign to %s"
#define ERROR_CANNOT_ASSIGN_TO_IMMUTABLE				33, "cannot assign to immutable variable"

#define NOTE_PREVIOUS_SYMBOL_DEFINITION					"previously defined here"
#define NOTE_DEFINED_HERE								"defined here"

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
	DataType* matched_dt;
	bool in_function;

	bool error;
	bool not_parsing_error;
	u64 error_count;
} Parser;

void parser_init(Parser* self, SrcFile* srcfile);
void parser_parse(Parser* self);

///// AST PRINTER /////
typedef struct {
	SrcFile* srcfile;
	int indent;
} AstPrinter;

void ast_printer_init(AstPrinter* self, SrcFile* srcfile);
void ast_printer_print(AstPrinter* self);

///// RESOLVER /////
typedef struct Scope {
	struct Scope* parent;
	Stmt** sym_tbl;
} Scope;

typedef struct {
	SrcFile* srcfile;
	Scope* global_scope;
	Scope* current_scope;
	bool dont_create_block_scope;

	bool error;
} Resolver;

void resolver_init(Resolver* self, SrcFile* srcfile);
void resolver_resolve(Resolver* self);

typedef struct {
	SrcFile* srcfile;
	bool error;
	u64 error_count;
} Checker;

void checker_init(Checker* self, SrcFile* srcfile);
void checker_check(Checker* self);

char* one_word_stmt_ty(Stmt* s);

///// MISC /////
char* aria_strsub(char* str, uint start, uint len);
char* aria_strapp(char* to, char* from);
char* aria_basename(char* fpath);
char* aria_notdir(char* fpath);

#define KEYWORDS_LEN 7
#define DIRECTIVES_LEN 1

extern char* executable_path_from_argv;
extern char* keywords[KEYWORDS_LEN];
extern char* directives[DIRECTIVES_LEN];

#define es \
	u64 COMBINE(_error_store,__LINE__) = self->error_count

#define er \
	if (self->error_count > COMBINE(_error_store,__LINE__)) return

#define ec \
	if (self->error_count > COMBINE(_error_store,__LINE__)) continue

#define ern \
	er null

#define chk(stmt) \
	es; stmt; ern;

// void
#define chkv(stmt) \
	es; stmt; er;

#define chkf(stmt) \
	es; stmt; er false;

// loop
#define chklp(stmt) \
	es; stmt; ec;

#endif	/* __ARIA_H */
