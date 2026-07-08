#include "ast.h"
#include "core.h"
#include "token.h"

Astnode* astnode_new(AstnodeKind kind, Span span, Span short_span) {
    Astnode* n = ALLOC_OBJ(Astnode);
    n->kind = kind;
    n->span = span;
    n->short_span = short_span;
    return n;
}

Astnode* astnode_field_new(Token* ident, Astnode* type) {
    Astnode* n = astnode_new(
        AST_FIELD, 
        span_from_two(ident->span, type->span),
        ident->span
    );
    n->field.ident = ident;
    n->field.type = type;
    return n;
}

Astnode* astnode_struct_import_new(
    Token* start, 
    Token* path, 
    Token* end,
    Srcfile* srcfile
) {
    Astnode* n = astnode_new(
        AST_STRUCT, 
        span_from_two(start->span, end->span), 
        path->span
    );
    n->strct.import = true;
    n->strct.imp.path = path;
    n->strct.imp.srcfile = srcfile;
    return n;
}

Astnode* astnode_struct_inline_new(Astnode** ast) {

}

Astnode* astnode_symbol_new(Token* ident) {
    Astnode* n = astnode_new(AST_SYM, ident->span, ident->span);
    n->sym.ident = ident;
    return n;
}

Astnode* astnode_vardecl_new(
    Token* start, 
    Token* ident,
    Astnode* type,
    // TODO: maybe remove equal?
    Token* equal,
    Astnode* init
) {
    Astnode* n = astnode_new(
        AST_VARDECL, 
        span_from_two(
            start->span, 
            init 
                ? init->span 
                : type->span
        ),
        ident->span
    );
    n->vardecl.ident = ident;
    n->vardecl.type = type;
    n->vardecl.equal = equal;
    n->vardecl.init = init;
    return n;
}

