#include "core.h"
#include "token.h"
#include "dbg.h"
#include "node.h"

static int indent;

// void dbg_print_tokens(Token* tokens, Srcfile* srcfile) {
//     src = srcfile;
//     bufloop(tokens, i) {
//         printf(
//             "\n%20s %20s %d,%d", 
//             tokenkind_strs[tokens[i].kind], 
//             span_tostring(tokens[i].span, p->src),
//             tokens[i].span.start,
//             tokens[i].span.end
//         );
//     }
//     printf("\ntokens: %lu", buflen(tokens));
// }

static void print_token(ParseCtx* p, TokenIndex idx) {
    Token* token = &p->src->tokens[idx];
    printf(
        "%.*s",
        (int)(token->span.end - token->span.start),
        &p->src->handle.contents[token->span.start]
    );
}

static void format() {
    printf("\n");
    for (int i = 0; i < indent; i++) {
        printf("    ");
    }
}

static inline Node* nd(ParseCtx* p, NodeIndex n) {
    return &p->src->nodes[n];
}

static void print_node(ParseCtx* p, NodeIndex node) {
    if (node == 0) {
        printf("nil");
        return;
    }

    Node* n = nd(p, node);
    switch (n->kind) {
        case AST_BLOCK:
        case AST_FIELD:
        case AST_FNDECL:
        case AST_VARDECL:
            format();
            break;
        default: break;
    }

    switch (n->kind) {
        case AST_BLOCK: {
            int count = p->src->nextra[n->lhs + 0];
            int value = p->src->nextra[n->lhs + 1];

            printf("(block ");
            indent++;
            for (int i = 0; i < count; i++) {
                print_node(p, p->src->nextra[n->lhs + 2 + i]);
            }
            format();
            printf("(yield ");
            print_node(p, value);
            printf(")");
            indent--;
            printf(")");
        } break;

        case AST_COMP: {
            printf("(comp ");
            print_node(p, n->lhs);
            printf(")");
        } break;

        case AST_EXPRSTMT: {
            printf("(exprstmt ");
            print_node(p, n->lhs);
            printf(")");
        } break;

        case AST_FIELD: {
            printf("(field ");
            print_token(p, n->lhs);
            printf(" : ");
            print_node(p, n->rhs);
            printf(")");
        } break;

        case AST_FNDECL: {
            int param_count = p->src->nextra[n->lhs + 0];
            int returntype = p->src->nextra[n->lhs + 1];
            int body = p->src->nextra[n->lhs + 2];

            printf("(func ");  
            print_token(p, n->rhs);
            printf(" (");
            for (int i = 0; i < param_count; i++) {
                if (i != 0) printf(", ");
                print_node(p, p->src->nextra[n->lhs + 3 + i]);
            }
            printf(") : ");
            print_node(p, returntype);

            printf(" ");
            print_node(p, body);

            printf(")");
        } break;

        case AST_PARAM: {
            printf("(");
            print_token(p, n->lhs);
            printf(" : ");
            print_node(p, n->rhs);
            printf(")");
        } break;

        case AST_STRUCT: {
            printf("(struct ");
            indent++;
            for (int i = 0; i < n->rhs; i++) {
                print_node(p, p->src->nextra[n->lhs + i]);
            }
            indent--;
            printf(")");
        } break;

        case AST_SYM: {
            printf("(sym ");
            print_token(p, n->lhs);
            printf(")");
        } break;

        case AST_VARDECL: {
            int kw = p->src->nextra[n->lhs + 0];
            int type = p->src->nextra[n->lhs + 1];
            int init = p->src->nextra[n->lhs + 2];
            printf("(vardecl ");
            print_token(p, n->rhs);
            if (type) {
                printf(" : ");
                print_node(p, type);
            }
            if (init) {
                printf(" = ");
                print_node(p, init);
            }
            printf(")");
        } break;
    }
}

void dbg_nodes(ParseCtx* p) {
    printf("\nname: %s", p->src->handle.path);
    printf("\ntokens: %lu", buflen(p->src->tokens));
    for (int i = 0; i < (int)buflen(p->src->tokens); i++) {
        printf(
            "\n  at tokens[%2d] -> %s", 
            i, 
            span_tostring(p->src->tokens[i].span, p->src)
        );
    }

    printf("\nnodes: %lu", buflen(p->src->nodes));
    for (int i = 0; i < (int)buflen(p->src->nodes); i++) {
        Node* n = &p->src->nodes[i];
        printf("\n  at nodes[%2d] -> k=%s, lhs=%d, rhs=%d", i, nodekind_strs[n->kind], n->lhs, n->rhs);
    }
    for (int i = 0; i < (int)buflen(p->src->nextra); i++) {
        printf("\n  at nextra[%2d] -> %d", i, p->src->nextra[i]);
    }

    Node* r = &p->src->nodes[1];
    for (int i = 0; i < r->rhs; i++) {
        print_node(p, p->src->nextra[r->lhs + i]);
    }
    printf("\n");
}
