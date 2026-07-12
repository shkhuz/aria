#ifndef AST_H 
#define AST_H

#include "types.h"
#include "srcfile.h"

typedef enum {
    AST_BLOCK,
    AST_COMP,
    AST_EXPRSTMT,
    AST_FIELD,
    AST_FUNC,
    AST_PARAM,
    AST_STRUCT,
    AST_SYM,
    AST_VARDECL,
} AstnodeKind;

struct Astnode {
    AstnodeKind kind;
    Span span;

    union {
        struct {
            Astnode** ast;
            Astnode* value;
        } block;

        struct {
            Astnode* child;
        } comp;

        struct {
            Astnode* child;
        } exprstmt;

        struct {
            TokenIndex ident;
            Astnode* type;
        } field;

        struct {
            TokenIndex ident;
            Astnode** params;
            Astnode* returntype;
            Astnode* body;
        } func;

        struct {
            TokenIndex ident;
            Astnode* type;
        } param;

        struct {
            bool import;
            union {
                struct {
                    TokenIndex path;
                    Srcfile* src;
                } imp;

                struct {
                    Astnode** ast;
                } inl;
            };
        } strct;

        struct {
            TokenIndex ident;
        } sym;

        struct {
            TokenIndex ident; 
            Astnode* type;
            TokenIndex equal;
            Astnode* init;
        } vardecl;
    };
};

#endif
