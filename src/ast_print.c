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

    if (astnode->kind == ASTNODE_STRUCT
        || astnode->kind == ASTNODE_FIELD
        || astnode->kind == ASTNODE_IF
        || astnode->kind == ASTNODE_IF_BRANCH
        || astnode->kind == ASTNODE_WHILE
        || astnode->kind == ASTNODE_IMPORT
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
        case ASTNODE_TYPESPEC_FUNC: {
            printf("(fn (");
            bufloop(astnode->typefunc.params, i) {
                print_node(astnode->typefunc.params[i]);
                if (i != buflen(astnode->typefunc.params)-1) printf(", ");
            }
            printf(") ");
            print_node(astnode->typefunc.ret_typespec);
            printf(")");
        } break;

        case ASTNODE_TYPESPEC_PTR: {
            printf("*");
            if (astnode->typeptr.immutable) printf("imm ");
            print_node(astnode->typeptr.child);
        } break;

        case ASTNODE_TYPESPEC_MULTIPTR: {
            printf("[*]");
            if (astnode->typeptr.immutable) printf("imm ");
            print_node(astnode->typeptr.child);
        } break;

        case ASTNODE_TYPESPEC_SLICE: {
            printf("[]");
            if (astnode->typeslice.immutable) printf("imm ");
            print_node(astnode->typeslice.child);
        } break;

        case ASTNODE_TYPESPEC_ARRAY: {
            printf("(tyarr [");
            print_node(astnode->typearray.size);
            printf("]");
            print_node(astnode->typearray.child);
            printf(")");
        } break;

        case ASTNODE_TYPESPEC_TUPLE: {
            printf("(tytup");
            bufloop(astnode->typetup.elems, i) {
                printf(" ");
                print_node(astnode->typetup.elems[i]);
            }
            printf(")");
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
            print_token(astnode->field.key);
            printf(" ");
            indent += 4;
            print_node(astnode->field.value);
            indent -= 4;
            printf(")");
        } break;

        case ASTNODE_INTEGER_LITERAL: {
            print_token(astnode->intl.token);
        } break;

        case ASTNODE_STRING_LITERAL: {
            print_token(astnode->strl.token);
        } break;

        case ASTNODE_ARRAY_LITERAL: {
            printf("(array ");
            if (astnode->arrayl.elem_type) print_node(astnode->arrayl.elem_type);
            else printf("_");
            bufloop(astnode->arrayl.elems, i) {
                printf(" ");
                print_node(astnode->arrayl.elems[i]);
            }
            printf(")");
        } break;

        case ASTNODE_TUPLE_LITERAL: {
            printf("(tuple");
            bufloop(astnode->tupl.elems, i) {
                printf(" ");
                print_node(astnode->tupl.elems[i]);
            }
            printf(")");
        } break;

        case ASTNODE_AGGREGATE_LITERAL: {
            printf("(aggliteral ");
            print_node(astnode->aggl.typespec);
            bufloop(astnode->strct.fields, i) {
                print_node(astnode->strct.fields[i]);
            }
            printf(")");
        } break;

        case ASTNODE_SYMBOL: {
            print_token(astnode->sym.identifier);
        } break;

        case ASTNODE_BUILTIN_SYMBOL: {
            print_token(astnode->bsym.identifier);
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

        case ASTNODE_WHILE: {
            printf("(while ");
            indent += 1;
            print_node(astnode->whloop.cond);
            format();
            print_node(astnode->whloop.mainbody);
            if (astnode->whloop.elsebody) {
                format();
                print_node(astnode->whloop.elsebody);
            }
            indent -= 1;
            printf(")");
        } break;

        case ASTNODE_BREAK: {
            printf("(break");
            if (astnode->brk.child) {
                printf(" ");
                print_node(astnode->brk.child);
            }
            printf(")");
        } break;

        case ASTNODE_CONTINUE: {
            printf("(continue)");
        } break;

        case ASTNODE_RETURN: {
            printf("(return");
            if (astnode->ret.child) {
                printf(" ");
                print_node(astnode->ret.child);
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
            printf("(");
            print_node(astnode->acc.left);
            printf(".");
            print_node(astnode->acc.right);
            printf(")");
        } break;

        case ASTNODE_UNOP: {
            printf("(");
            switch (astnode->unop.kind) {
                case UNOP_NEG: printf("neg "); break;
                case UNOP_BOOLNOT: printf("boolnot "); break;
                case UNOP_ADDR: printf("addr "); break;
            }
            print_node(astnode->unop.child);
            printf(")");
        } break;

        case ASTNODE_DEREF: {
            printf("(deref ");
            print_node(astnode->unop.child);
            printf(")");
        } break;

        case ASTNODE_INDEX: {
            printf("(index ");
            print_node(astnode->idx.left);
            printf(" ");
            print_node(astnode->idx.idx);
            printf(")");
        } break;

        case ASTNODE_ARITH_BINOP: {
            printf("(");
            switch (astnode->arthbin.kind) {
                case ARITH_BINOP_ADD: printf("add "); break;
                case ARITH_BINOP_SUB: printf("sub "); break;
                case ARITH_BINOP_MUL: printf("mul "); break;
                case ARITH_BINOP_DIV: printf("div "); break;
                case ARITH_BINOP_REM: printf("rem "); break;
            }
            print_node(astnode->arthbin.left);
            printf(" ");
            print_node(astnode->arthbin.right);
            printf(")");
        } break;

        case ASTNODE_BOOL_BINOP: {
            printf("(");
            switch (astnode->arthbin.kind) {
                case BOOL_BINOP_AND: printf("and "); break;
                case BOOL_BINOP_OR:  printf("or "); break;
            }
            print_node(astnode->arthbin.left);
            printf(" ");
            print_node(astnode->arthbin.right);
            printf(")");
        } break;

        case ASTNODE_CMP_BINOP: {
            printf("(");
            switch (astnode->cmpbin.kind) {
                case CMP_BINOP_EQ: printf("eq "); break;
                case CMP_BINOP_NE: printf("ne "); break;
                case CMP_BINOP_LT: printf("lt "); break;
                case CMP_BINOP_GT: printf("gt "); break;
                case CMP_BINOP_LE: printf("le "); break;
                case CMP_BINOP_GE: printf("ge "); break;
            }
            print_node(astnode->cmpbin.left);
            printf(" ");
            print_node(astnode->cmpbin.right);
            printf(")");
        } break;

        case ASTNODE_ASSIGN: {
            printf("(assign ");
            print_node(astnode->assign.left);
            printf(" ");
            print_node(astnode->assign.right);
            printf(")");
        } break;

        case ASTNODE_CAST: {
            printf("(cast ");
            print_node(astnode->cast.left);
            printf(" ");
            print_node(astnode->cast.right);
            printf(")");
        } break;

        case ASTNODE_IMPORT: {
            printf("(import ");
            print_token(astnode->import.arg);
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
            print_node(astnode->funcdef.header);
            printf(" ");
            indent += 4;
            indent_block = false;
            print_node(astnode->funcdef.body);
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

        case ASTNODE_STRUCT: {
            indent += 4;
            printf("(struct ");
            print_token(astnode->strct.identifier);
            bufloop(astnode->strct.fields, i) {
                print_node(astnode->strct.fields[i]);
            }
            printf(")");
            indent -= 4;
        } break;
    }
}

void ast_print(AstNode** astnodes) {
    bufloop(astnodes, i) {
        print_node(astnodes[i]);
    }
}
