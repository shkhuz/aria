#ifndef _ARIA_H
#define _ARIA_H

#include "arpch.h"
#include "util/util.h"
#include "ds/ds.h"

typedef struct {
    bool error;
    Ast* ast;
} AstOutput;

AstOutput build_ast(const char* fpath);
bool check_ast(Ast* ast);

typedef struct {
    bool error;
    Token** tokens;
} TokenOutput;

typedef struct {
	File* srcfile;
	Token** tokens;

	char* start;
	char* current;
	u64 line;

	char* last_newline;
    bool error_state;
} Lexer;

TokenOutput lex(Lexer* self, File* srcfile);

typedef enum {
    ERRLOC_GLOBAL,
    ERRLOC_FUNCTION_HEADER,
    ERRLOC_BLOCK,
    ERRLOC_NONE,
} ErrorLocation;

typedef struct {
    Ast* ast;
    u64 tokens_idx;

    bool in_function;
    bool error_panic;
    u64 error_count;
    ErrorLocation error_loc;
    bool error_state;
} Parser;

AstOutput parse(Parser* self, File* srcfile, Token** tokens);

typedef struct {
    Ast* ast;
    u64 indent;
    bool first_stmt;
} AstDebugger;

void ast_debug(AstDebugger* self, Ast* ast);

typedef struct {
    Ast* ast;
    Scope* global_scope;
    Scope* current_scope;
    bool error_state;
    u64 error_count;
} Resolver;

bool resolve_ast(Resolver* self, Ast* ast);

typedef struct {
    Ast* ast;
    Stmt* enclosed_function;
    u64 error_count;
    bool error_state;
} TypeChecker;

bool type_check_ast(TypeChecker* self, Ast* ast);

typedef struct {
    char* a;
    char* b;
} TwoStringMap;

typedef struct {
    Ast* ast;
    Stmt* enclosed_function;

    char* code;
    u64 indent;

    u64 next_local_var_offset;
    u64 next_param_stack_offset;

    TwoStringMap** static_strings;
    u64 next_free_string_label;
} CodeGenerator;

void gen_code_for_ast(CodeGenerator* self, Ast* ast, const char* fpath);

#endif /* _ARIA_H */

