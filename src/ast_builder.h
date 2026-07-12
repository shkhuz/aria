static Span tkspan(ParseCtx* p, TokenIndex idx) {
    return p->src->tokens[idx].span;
}

static Astnode* astnode_new(AstnodeKind kind, Span span) {
    Astnode* n = ALLOC_OBJ(Astnode);
    n->kind = kind;
    n->span = span;
    return n;
}

static Astnode* astnode_block_new(
    ParseCtx* p,
    TokenIndex start, 
    Astnode** ast, 
    Astnode* value,
    TokenIndex end
) {
    Astnode* n = astnode_new(
        AST_BLOCK, 
        span_from_two(tkspan(p, start), tkspan(p, end))
    );
    n->block.ast = ast;
    n->block.value = value;
    return n;
}

static Astnode* astnode_comp_new(
    ParseCtx* p, 
    TokenIndex start, 
    Astnode* child
) {
    Astnode* n = astnode_new(
        AST_COMP, 
        span_from_two(tkspan(p, start), child->span)
    );
    n->comp.child = child;
    return n;
}

static Astnode* astnode_exprstmt_new(ParseCtx* p, Astnode* child) {
    Astnode* n = astnode_new(AST_EXPRSTMT, child->span);
    n->exprstmt.child = child;
    return n;
}

static Astnode* astnode_field_new(
    ParseCtx* p, 
    TokenIndex ident, 
    Astnode* type
) {
    Astnode* n = astnode_new(
        AST_FIELD, 
        span_from_two(tkspan(p, ident), type->span)
    );
    n->field.ident = ident;
    n->field.type = type;
    return n;
}

static Astnode* astnode_func_new(
    ParseCtx* p,
    TokenIndex start,
    TokenIndex ident,
    Astnode** params,
    Astnode* returntype,
    Astnode* body
) {
    Astnode* n = astnode_new(
        AST_FUNC,
        span_from_two(tkspan(p, start), body->span)
    );
    n->func.ident = ident;
    n->func.params = params;
    n->func.returntype = returntype;
    n->func.body = body;
    return n;
}

static Astnode* astnode_param_new(
    ParseCtx* p, 
    TokenIndex ident, 
    Astnode* type
) {
    Astnode* n = astnode_new(
        AST_PARAM,
        span_from_two(tkspan(p, ident), type->span)
    );
    n->param.ident = ident;
    n->param.type = type;
    return n;
}

static Astnode* astnode_struct_import_new(
    ParseCtx* p, 
    TokenIndex start, 
    TokenIndex path, 
    TokenIndex end,
    Srcfile* src
) {
    Astnode* n = astnode_new(
        AST_STRUCT, 
        span_from_two(tkspan(p, start), tkspan(p, end)) 
    );
    n->strct.import = true;
    n->strct.imp.path = path;
    n->strct.imp.src = src;
    return n;
}

static Astnode* astnode_struct_inline_new(
    ParseCtx* p, 
    TokenIndex start, 
    Astnode** ast, 
    TokenIndex end
) {
    Astnode* n = astnode_new(
        AST_STRUCT,
        span_from_two(tkspan(p, start), tkspan(p, end))
    );
    n->strct.import = false;
    n->strct.inl.ast = ast;
    return n;
}

static Astnode* astnode_symbol_new(ParseCtx* p, TokenIndex ident) {
    Astnode* n = astnode_new(AST_SYM, tkspan(p, ident));
    n->sym.ident = ident;
    return n;
}

static Astnode* astnode_vardecl_new(
    ParseCtx* p, 
    TokenIndex start, 
    TokenIndex ident,
    Astnode* type,
    // TODO: maybe remove equal?
    TokenIndex equal,
    Astnode* init
) {
    Astnode* n = astnode_new(
        AST_VARDECL, 
        span_from_two(tkspan(p, start), init ? init->span : type->span)
    );
    n->vardecl.ident = ident;
    n->vardecl.type = type;
    n->vardecl.equal = equal;
    n->vardecl.init = init;
    return n;
}

