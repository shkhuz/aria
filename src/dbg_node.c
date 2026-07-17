#include "core.h"
#include "token.h"
#include "dbg.h"
#include "node.h"
#include "compile.h"

static int indent;

void dbg_print_tokens(Srcfile* src) {
    listloop(src->tokens, i) {
        Token* t = tk(src, i);
        printf(
            "%15s '%s' %d,%d, extra=%d\n", 
            tokenkind_strs[t->kind], 
            span_tostring(t->span, src),
            t->span.start,
            t->span.end,
            t->extra
        );
    }
}

static void print_token(ParseCtx* p, TokenIndex idx) {
    Span span = tk(p->src, idx)->span;
    printf(
        "%.*s",
        (int)(span.end - span.start),
        &p->src->handle.contents[span.start]
    );
}

static void format() {
    printf("\n");
    for (int i = 0; i < indent; i++) {
        printf("    ");
    }
}

static void print_node(ParseCtx* p, NodeIndex node) {
    if (node == 0) {
        printf("nil");
        return;
    }

    Node* n = nd(p->src, node);
    switch (n->kind) {
        case AST_BLOCK:
        case AST_FIELD:
        case AST_FNDECL:
        case AST_VARDECL:
        case AST_EXPRSTMT:
            format();
            break;
        default: break;
    }

    switch (n->kind) {
        case AST_BLOCK: {
            int count = listget(p->src->nextra, n->lhs + 0);
            int value = listget(p->src->nextra, n->lhs + 1);

            printf("(block ");
            indent++;
            for (int i = 0; i < count; i++) {
                print_node(p, listget(p->src->nextra, n->lhs + 2 + i));
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
            int param_count = listget(p->src->nextra, n->lhs + 0);
            int returntype = listget(p->src->nextra, n->lhs + 1);
            int body = listget(p->src->nextra, n->lhs + 2);

            printf("(func ");  
            print_token(p, n->rhs);
            printf(" (");
            for (int i = 0; i < param_count; i++) {
                if (i != 0) printf(", ");
                print_node(p, listget(p->src->nextra, n->lhs + 3 + i));
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
                print_node(p, listget(p->src->nextra, n->lhs + i));
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
            int kw = listget(p->src->nextra, n->lhs + 0);
            int type = listget(p->src->nextra, n->lhs + 1);
            int init = listget(p->src->nextra, n->lhs + 2);
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
    for (int i = 0; i < (int)listlen(p->src->tokens); i++) {
        printf(
            "  at tokens[%2d] -> %s\n", 
            i, 
            span_tostring(tk(p->src, i)->span, p->src)
        );
    }

    for (int i = 0; i < (int)listlen(p->src->nodes); i++) {
        Node* n = &listget(p->src->nodes, i);
        printf("  at nodes[%2d] -> k=%s, lhs=%d, rhs=%d\n", i, nodekind_strs[n->kind], n->lhs, n->rhs);
    }
    for (int i = 0; i < (int)listlen(p->src->nextra); i++) {
        printf("  at nextra[%2d] -> %d\n", i, listget(p->src->nextra, i));
    }

    Node* r = &listget(p->src->nodes, 1);
    for (int i = 0; i < r->rhs; i++) {
        print_node(p, listget(p->src->nextra, r->lhs + i));
    }
}
