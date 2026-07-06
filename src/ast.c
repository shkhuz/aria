#include "ast.h"
#include "core.h"
#include "token.h"

Astnode* astnode_new(AstnodeKind kind, Span span) {
    Astnode* n = ALLOC_OBJ(Astnode);
    n->kind = kind;
    n->span = span;
    n->short_span = span;
    return n;
}

Astnode* astnode_struct_import_new(Token* path) {
    Astnode* n = astnode_new(AST_STRUCT, path->span);
    n->strct.import = true;
    n->strct.imp.path = path;
    return n;
}

Astnode* astnode_struct_inline_new(Astnode** ast) {

}

Astnode* astnode_vardecl_new(
    Token* start, 
    Token* ident,
    Astnode* typespec,
    Token* equal,
    Astnode* initializer
) {
    Astnode* n = astnode_new(AST_VARDECL, start->span);
    n->vardecl.ident = ident;
    n->vardecl.initializer = initializer;
    return n;
}

Astnode* astnode_symbol_new(Token* ident) {
    Astnode* n = astnode_new(AST_SYM, ident->span);
    n->sym.ident = ident;
    return n;
}
