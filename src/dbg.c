#include "core.h"
#include "token.h"
#include "dbg.h"
#include "ast.h"

void dbg_print_tokens(Token** tokens) {
    bufloop(tokens, i) {
        printf(
            "\n%20s %20s %lu,%lu", 
            tokenkind_strs[tokens[i]->kind], 
            span_tostring(tokens[i]->span),
            tokens[i]->span.start,
            tokens[i]->span.end
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

static void format() {
    printf("\n");
    for (int i = 0; i < indent; i++) {
        printf("    ");
    }
}

static void print_astnode(Astnode* n) {
    if (!n) {
        printf("nil");
        return;
    }

    switch (n->kind) {
        case AST_BLOCK:
        case AST_FIELD:
        case AST_FUNC:
        case AST_VARDECL:
            format();
            break;
        default: break;
    }

    switch (n->kind) {
        case AST_BLOCK: {
            printf("(block ");
            indent++;
            bufloop(n->block.ast, i) {
                print_astnode(n->block.ast[i]);
            }
            format();
            printf("(yield ");
            print_astnode(n->block.value);
            printf(")");
            indent--;
            printf(")");
        } break;

        case AST_EXPRSTMT: {
            printf("(exprstmt ");
            print_astnode(n->exprstmt.expr);
            printf(")");
        } break;

        case AST_FIELD: {
            printf("(field ");
            print_token(n->field.ident);
            printf(" : ");
            print_astnode(n->field.type);
            printf(")");
        } break;

        case AST_FUNC: {
            printf("(func ");  
            print_token(n->func.ident);
            printf(" (");
            bufloop(n->func.params, i) {
                if (i != 0) printf(", ");
                print_astnode(n->func.params[i]);
            }
            printf(") ");
            print_astnode(n->func.returntype);

            printf(" ");
            print_astnode(n->func.body);

            printf(")");
        } break;

        case AST_PARAM: {
            printf("(");
            print_token(n->param.ident);
            printf(" : ");
            print_astnode(n->param.type);
            printf(")");
        } break;

        case AST_STRUCT: {
            if (n->strct.import) {
                printf("(struct-import ");
                print_token(n->strct.imp.path);
                printf(")");
            } else {
                printf("(struct ");
                indent++;
                bufloop(n->strct.inl.ast, i) {
                    print_astnode(n->strct.inl.ast[i]);
                }
                indent--;
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
