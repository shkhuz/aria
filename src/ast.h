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
    Span short_span;

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
            Token* ident;
            Astnode* type;
        } field;

        struct {
            Token* ident;
            Astnode** params;
            Astnode* returntype;
            Astnode* body;
        } func;

        struct {
            Token* ident;
            Astnode* type;
        } param;

        struct {
            bool import;
            union {
                struct {
                    Token* path;
                    Srcfile* src;
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

Astnode* astnode_block_new(
    Token* start, 
    Astnode** ast, 
    Astnode* value,
    Token* end
);
Astnode* astnode_comp_new(Token* start, Astnode* child);
Astnode* astnode_exprstmt_new(Astnode* child);
Astnode* astnode_field_new(Token* ident, Astnode* type);
Astnode* astnode_func_new(
    Token* start,
    Token* ident,
    Astnode** params,
    Astnode* returntype,
    Astnode* body
);
Astnode* astnode_param_new(Token* ident, Astnode* type);
Astnode* astnode_struct_import_new(
    Token* start, 
    Token* path, 
    Token* end,
    Srcfile* src
);
Astnode* astnode_struct_inline_new(
    Token* start, 
    Astnode** ast, 
    Token* end
);
Astnode* astnode_symbol_new(Token* ident);
Astnode* astnode_vardecl_new(
    Token* start, 
    Token* ident,
    Astnode* type,
    Token* equal,
    Astnode* init
);

#endif
