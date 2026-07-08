#include "core.h"
#include "token.h"
#include "dbg.h"
#include "ast.h"

void dbg_print_tokens(Token** tokens) {
    bufloop(tokens, i) {
        printf(
            "\n%20s %s", 
            tokenkind_strs[tokens[i]->kind], 
            span_tostring(tokens[i]->span)
        );
    }
    printf("\ntokens: %lu", buflen(tokens));
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
        case AST_FIELD:
            printf("\n");
            break;
        default: break;
    }

    switch (n->kind) {
        case AST_FIELD: {
            printf("(field ");
            print_token(n->field.ident);
            printf(" : ");
            print_astnode(n->field.type);
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

        case AST_VARDECL: {
            printf("(vardecl ");
            print_token(n->vardecl.ident);
            if (n->vardecl.type) {
                printf(" : ");
                print_astnode(n->vardecl.type);
            }
            if (n->vardecl.init) {
                printf(" = ");
                print_astnode(n->vardecl.init);
            }
            printf(")");
        } break;
    }
}

void dbg_print_ast(Astnode** ast) {
    bufloop(ast, i) {
        print_astnode(ast[i]);
    }
    printf("\n");
}
