#ifndef AST_H 
#define AST_H

#include "srcfile.h"

typedef struct Token Token;
typedef struct Astnode Astnode;

typedef enum {
    AST_VARDECL,
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
    };
};

Astnode* astnode_vardecl_new(
    Token* start, 
    Token* ident,
    Astnode* typespec,
    Token* equal,
    Astnode* initializer
);

#endif
