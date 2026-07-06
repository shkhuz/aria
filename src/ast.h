#ifndef AST_H 
#define AST_H

#include "srcfile.h"

typedef struct Token Token;
typedef struct Astnode Astnode;

typedef enum {
    AST_VARDECL,
    AST_STRUCT,
    AST_SYM,
} AstnodeKind;

struct Astnode {
    AstnodeKind kind;
    Span span;
    Span short_span;

    union {
        struct {
            Token* ident; 
            Astnode* typespec;
            Token* equal;
            Astnode* initializer;
        } vardecl;

        struct {
            bool import;
            union {
                struct {
                    Token* path;
                } imp;

                struct {
                    Astnode** ast;
                } inl;
            };
        } strct;

        struct {
            Token* ident;
        } sym;
    };
};

Astnode* astnode_vardecl_new(
    Token* start, 
    Token* ident,
    Astnode* typespec,
    Token* equal,
    Astnode* initializer
);
Astnode* astnode_struct_import_new(Token* path);
Astnode* astnode_struct_inline_new(Astnode** ast);
Astnode* astnode_symbol_new(Token* ident);

#endif
