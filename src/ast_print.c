#include "ast_print.h"
#include "buf.h"
#include "compile.h"

static int indent;
static bool print_formatting = false;
static bool indent_block = true;

static void print_node(AstNode* astnode);

static void print_token(Token* token) {
    printf(
        "%.*s",
        (int)(token->span.end - token->span.start),
        &token->span.srcfile->handle.contents[token->span.start]);
}

static inline void format() {
    printf("\n");
    for (int i = 0; i < indent; i++) printf(" ");
}

static void print_node(AstNode* astnode) {
    if (!astnode) {
        printf("nil");
        return;
    }

    if (astnode->kind == ASTNODE_TYPESPEC_STRUCT
        || astnode->kind == ASTNODE_FIELD
        || astnode->kind == ASTNODE_IF
        || astnode->kind == ASTNODE_IF_BRANCH
        || astnode->kind == ASTNODE_TYPEDECL
        || astnode->kind == ASTNODE_FUNCTION_DEF
        || astnode->kind == ASTNODE_VARIABLE_DECL
        || astnode->kind == ASTNODE_EXPRSTMT) {
        if (print_formatting) {
            format();
        }
        else print_formatting = true;
        indent_block = true;
    }

    switch (astnode->kind) {
        case ASTNODE_TYPESPEC_PTR: {
            printf("*");
            if (astnode->typeptr.immutable) printf("imm ");
            print_node(astnode->typeptr.child);
        } break;

        case ASTNODE_TYPESPEC_STRUCT: {
            indent += 4;
            printf("(struct");
            bufloop(astnode->typestruct.fields, i) {
                print_node(astnode->typestruct.fields[i]);
            }
            bufloop(astnode->typestruct.stmts, i) {
                print_node(astnode->typestruct.stmts[i]);
            }
            printf(")");
            indent -= 4;
        } break;

        case ASTNODE_GENERIC_TYPESPEC: {
            print_node(astnode->genty.left);
            printf("<");
            bufloop(astnode->genty.args, i) {
                print_node(astnode->genty.args[i]);
                if (i != buflen(astnode->genty.args)-1) printf(", ");
            }
            printf(">");
        } break;

        case ASTNODE_DIRECTIVE: {
            printf("(@");
            print_token(astnode->directive.callee);
            bufloop(astnode->funcc.args, i) {
                printf(" ");
                print_node(astnode->funcc.args[i]);
            }
            printf(")");
        } break;

        case ASTNODE_FIELD: {
            printf("(field ");
            print_token(astnode->field.identifier);
            printf(" ");
            indent += 4;
            print_node(astnode->field.typespec);
            indent -= 4;
            printf(")");
        } break;

        case ASTNODE_INTEGER_LITERAL: {
            print_token(astnode->intl.token);
        } break;

        case ASTNODE_SYMBOL: {
            print_token(astnode->sym.identifier);
        } break;

        case ASTNODE_SCOPED_BLOCK: {
            bool indented = indent_block;
            if (indent_block) indent += 4;
            printf("(block");
            bufloop(astnode->blk.stmts, i) {
                print_node(astnode->blk.stmts[i]);
            }
            if (astnode->blk.val) {
                format();
                printf("(yield ");
                print_node(astnode->blk.val);
                printf(")");
            }
            printf(")");
            // Here indent_block may be changed by an external function, that's
            // why we save `indent_block` at the start
            if (indented) indent -= 4;
        } break;

        case ASTNODE_IF_BRANCH: {
            switch (astnode->ifbr.kind) {
                case IFBR_IF: printf("if "); break;
                case IFBR_ELSEIF: printf("else if "); break;
                case IFBR_ELSE: printf("else "); break;
            }
            if (astnode->ifbr.kind != IFBR_ELSE) {
                print_node(astnode->ifbr.cond);
                printf(" ");
            }
            print_node(astnode->ifbr.body);
        } break;

        case ASTNODE_IF: {
            printf("(");
            indent += 1;
            print_formatting = false;
            print_node(astnode->iff.ifbr);
            bufloop(astnode->iff.elseifbr, i) {
                print_node(astnode->iff.elseifbr[i]);
            }
            if (astnode->iff.elsebr) print_node(astnode->iff.elsebr);
            indent -= 1;
            printf(")");
        } break;

        case ASTNODE_RETURN: {
            printf("(return");
            if (astnode->ret.operand) {
                printf(" ");
                print_node(astnode->ret.operand);
            }
            printf(")");
        } break;

        case ASTNODE_FUNCTION_CALL: {
            printf("(call ");
            print_node(astnode->funcc.callee);
            bufloop(astnode->funcc.args, i) {
                printf(" ");
                print_node(astnode->funcc.args[i]);
            }
            printf(")");
        } break;

        case ASTNODE_ACCESS: {
            print_node(astnode->acc.left);
            printf(".");
            print_token(astnode->acc.right);
        } break;

        case ASTNODE_TYPEDECL: {
            printf("(typedecl ");
            print_token(astnode->typedecl.identifier);
            printf(" ");
            indent += 4;
            print_node(astnode->typedecl.right);
            indent -= 4;
            printf(")");
        } break;

        case ASTNODE_FUNCTION_HEADER: {
            printf("fn ");
            print_token(astnode->funch.identifier);
            printf(" (");
            bufloop(astnode->funch.params, i) {
                print_node(astnode->funch.params[i]);
                if (i != buflen(astnode->funch.params)-1) printf(", ");
            }
            printf(") ");
            print_node(astnode->funch.ret_typespec);
        } break;

        case ASTNODE_FUNCTION_DEF: {
            printf("(");
            print_node(astnode->funcd.header);
            printf(" ");
            indent += 4;
            indent_block = false;
            print_node(astnode->funcd.body);
            indent -= 4;
            printf(")");
        } break;

        case ASTNODE_VARIABLE_DECL: {
            printf("(storage ");
            if (astnode->vard.immutable) printf("imm ");
            else printf("mut ");
            print_token(astnode->vard.identifier);
            printf(" ");
            print_node(astnode->vard.typespec);
            printf(" = ");
            indent += 4;
            indent_block = false;
            print_node(astnode->vard.initializer);
            indent -= 4;
            printf(")");
        } break;

        case ASTNODE_PARAM_DECL: {
            print_token(astnode->paramd.identifier);
            printf(" ");
            print_node(astnode->paramd.typespec);
        } break;

        case ASTNODE_EXPRSTMT: {
            printf("(expr ");
            indent += 4;
            indent_block = false;
            print_node(astnode->exprstmt);
            indent -= 4;
            printf(")");
        } break;
    }
}

void ast_print(AstNode** astnodes) {
    bufloop(astnodes, i) {
        print_node(astnodes[i]);
    }
}
