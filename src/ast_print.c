#include "ast_print.h"
#include "buf.h"
#include "printf.h"

static int indent;
static bool print_formatting = false;
static bool indent_block = true;

static void print_node(AstNode* astnode);

static void print_symbol(AstNode* astnode) {
    aria_printf("%to", astnode->sym.identifier);
}

static void print_node(AstNode* astnode) {
    if (!astnode) {
        aria_printf("nil");
        return;
    }
    
    if (astnode->kind == ASTNODE_IF
        || astnode->kind == ASTNODE_IF_BRANCH
        || astnode->kind == ASTNODE_FUNCTION_DEF
        || astnode->kind == ASTNODE_VARIABLE_DECL
        || astnode->kind == ASTNODE_EXPRSTMT) {
        if (print_formatting) {
            aria_printf("\n");
            for (int i = 0; i < indent; i++) aria_printf(" ");

        }
        else print_formatting = true; 
        indent_block = true;
    }
    
    switch (astnode->kind) {
        case ASTNODE_TYPESPEC_SYMBOL: {
            print_symbol(astnode);
        } break;
        
        case ASTNODE_TYPESPEC_PTR: {
            aria_printf("*");
            print_node(astnode->typeptr.child);
        } break;
        
        case ASTNODE_INTEGER_LITERAL: {
            aria_printf("%to", astnode->intl.token);
        } break;
        
        case ASTNODE_SYMBOL: {
            print_symbol(astnode);
        } break;
        
        case ASTNODE_SCOPED_BLOCK: {
            bool indented = indent_block;
            if (indent_block) indent += 4; 
            aria_printf("(block");
            bufloop(astnode->blk.stmts, i) {
                print_node(astnode->blk.stmts[i]);
            }
            aria_printf(")");
            // Here indent_block may be changed by an external function, that's 
            // why we save `indent_block` at the start
            if (indented) indent -= 4;
        } break;
        
        case ASTNODE_IF_BRANCH: {
            switch (astnode->ifbr.kind) {
                case IFBR_IF: aria_printf("if "); break;
                case IFBR_ELSEIF: aria_printf("else if "); break;
                case IFBR_ELSE: aria_printf("else "); break;
            }
            if (astnode->ifbr.kind != IFBR_ELSE) {
                print_node(astnode->ifbr.cond);
                aria_printf(" ");
            }
            print_node(astnode->ifbr.body);
        } break;
        
        case ASTNODE_IF: {
            aria_printf("(");
            indent += 1;
            print_formatting = false;
            print_node(astnode->iff.ifbr);
            bufloop(astnode->iff.elseifbr, i) {
                print_node(astnode->iff.elseifbr[i]);
            }
            if (astnode->iff.elsebr) print_node(astnode->iff.elsebr);
            indent -= 1;
            aria_printf(")");
        } break;
        
        case ASTNODE_RETURN: {
            aria_printf("(return");
            if (astnode->ret.operand) {
                aria_printf(" ");
                print_node(astnode->ret.operand);
            }
            aria_printf(")");
        } break;
        
        case ASTNODE_FUNCTION_CALL: {
            aria_printf("(call ");
            print_node(astnode->funcc.callee);
            bufloop(astnode->funcc.args, i) {
                aria_printf(" ");
                print_node(astnode->funcc.args[i]);
            }
            aria_printf(")");
        } break;
        
        case ASTNODE_FUNCTION_HEADER: {
		    aria_printf("fn %to (", astnode->funch.identifier);
		    bufloop(astnode->funch.params, i) {
		        print_node(astnode->funch.params[i]);
                if (i != buflen(astnode->funch.params)-1) aria_printf(", ");
		    }
            aria_printf(") ");
            print_node(astnode->funch.ret_typespec);
        } break;
        
        case ASTNODE_FUNCTION_DEF: {
            aria_printf("(");
            print_node(astnode->funcd.header);
            aria_printf(" ");
            indent += 4;
            indent_block = false;
            print_node(astnode->funcd.body);
            indent -= 4;
            aria_printf(")");
        } break;
        
        case ASTNODE_VARIABLE_DECL: {
            aria_printf("(storage ");
            if (astnode->vard.immutable) aria_printf("imm ");
            else aria_printf("mut ");
            aria_printf("%to ", astnode->vard.identifier);
            print_node(astnode->vard.typespec);
            aria_printf(" = ");
            indent += 4;
            indent_block = false;
            print_node(astnode->vard.initializer);
            indent -= 4;
            aria_printf(")");
        } break;
        
        case ASTNODE_PARAM_DECL: {
            aria_printf("%to ", astnode->paramd.identifier);
            print_node(astnode->paramd.typespec);
        } break;
        
        case ASTNODE_EXPRSTMT: {
            aria_printf("(expr ");
            indent += 4;
            indent_block = false;
            print_node(astnode->exprstmt);
            indent -= 4;
            aria_printf(")");
        } break;
    }
}

void ast_print(AstNode** astnodes) {
    bufloop(astnodes, i) {
        print_node(astnodes[i]);       
    }
}
