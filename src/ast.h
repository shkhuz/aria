#ifndef AST_H 
#define AST_H

#include "srcfile.h"

typedef struct Token Token;
typedef struct Astnode Astnode;

typedef enum {
    AST_FIELD,
    AST_STRUCT,
    AST_SYM,
    AST_VARDECL,
} AstnodeKind;

struct Astnode {
    AstnodeKind kind;
    Span span;
    Span short_span;

    union {
        struct {
            Token* ident;
            Astnode* type;
        } field;

        struct {
            bool import;
            union {
                struct {
                    Token* path;
                    Srcfile* srcfile;
                } imp;

                struct {
                    Astnode** ast;
                } inl;
            };
        } strct;

        struct {
            Token* ident;
        } sym;

        struct {
            Token* ident; 
            Astnode* type;
            Token* equal;
            Astnode* init;
        } vardecl;
    };
};

Astnode* astnode_field_new(Token* ident, Astnode* type);
Astnode* astnode_struct_import_new(
    Token* start, 
    Token* path, 
    Token* end,
    Srcfile* srcfile
);
Astnode* astnode_struct_inline_new(Astnode** ast);
Astnode* astnode_symbol_new(Token* ident);
Astnode* astnode_vardecl_new(
    Token* start, 
    Token* ident,
    Astnode* type,
    Token* equal,
    Astnode* init
);

#endif
