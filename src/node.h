#ifndef AST_H 
#define AST_H

#include "types.h"
#include "core.h"
#include "srcfile.h"

typedef enum {
    #define WRAP(KIND) KIND,
    #include "nodekind.def"
    #undef WRAP
} NodeKind;

extern const char* nodekind_strs[];

struct Node {
    NodeKind kind;
    Span span;
    int lhs, rhs;
};

// union {
//     struct {
//         Astnode** ast;
//         Astnode* value;
//     } block;

//     struct {
//         Astnode* child;
//     } comp;

//     struct {
//         Astnode* child;
//     } exprstmt;

//     struct {
//         TokenIndex ident;
//         Astnode* type;
//     } field;

//     struct {
//         TokenIndex ident;
//         Astnode** params;
//         Astnode* returntype;
//         Astnode* body;
//     } func;

//     struct {
//         TokenIndex ident;
//         Astnode* type;
//     } param;

//     struct {
//     } root;

//     struct {
//         bool import;
//         union {
//             struct {
//                 TokenIndex path;
//                 Srcfile* src;
//             } imp;

//             struct {
//                 Astnode** ast;
//             } inl;
//         };
//     } strct;

//     struct {
//         TokenIndex ident;
//     } sym;

//     struct {
//         TokenIndex ident; 
//         Astnode* type;
//         TokenIndex equal;
//         Astnode* init;
//     } vardecl;
// };
#endif
