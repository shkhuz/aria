#include "core.h"
#include "token.h"
#include "dbg.h"
#include "ast.h"

void dbg_print_tokens(Token** tokens) {
    bufloop(tokens, i) {
        printf(
            "%20s %s\n", 
            tokenkind_strs[tokens[i]->kind], 
            span_tostring(tokens[i]->span)
        );
    }
    printf("tokens: %lu\n", buflen(tokens));
}

static int indent;

static void print_token(Token* token) {
    printf(
        "%.*s",
        (int)(token->span.end - token->span.start),
        &token->span.srcfile->handle.contents[token->span.start]
    );
}

static void print_astnode(Astnode* n) {
    if (!n) {
        printf("nil");
        return;
    }

    switch (n->kind) {
        case AST_VARDECL:
            printf("\n");
            break;
        default: break;
    }

    switch (n->kind) {
        case AST_VARDECL: {
            printf("(vardecl ");
            print_token(n->vardecl.ident);
            if (n->vardecl.initializer) {
                printf(" = ");
                print_astnode(n->vardecl.initializer);
            }
            printf(")");
        } break;

        case AST_STRUCT: {
            printf("(struct-import ");
            if (n->strct.import) {
                print_token(n->strct.imp.path);
                printf(")");
            }
        } break;

        case AST_SYM: {
            printf("(sym ");
            print_token(n->sym.ident);
            printf(")");
        } break;
    }
}

void dbg_print_ast(Astnode** ast) {
    bufloop(ast, i) {
        print_astnode(ast[i]);
    }
}
