#include "cg.h"
#include "buf.h"
#include "stri.h"
#include "msg.h"

typedef void (*PFunc)(CgContext*, char*, ...);

typedef enum {
    WHILE_COND,
    WHILE_END,
} WhileStmtLabel;

static void cg_stmt(CgContext* c, Stmt* stmt);
static void cg_function_stmt(CgContext* c, Stmt* stmt);
static void cg_type(CgContext* c, Type* type);
static void cg_p(CgContext* c, char* fmt, ...);
static void cg_pn(CgContext* c, char* fmt, ...);
/* static void cg_tasmp(CgContext* c, char* fmt, ...); */
/* static void cg_asmw(CgContext* c, char* str); */
/* static void cg_nasmw(CgContext* c, char* str); */
/* static void cg_asmplb(CgContext* c, char* labelfmt, ...); */
/* static void cg_asmwlb(CgContext* c, char* label); */
static void cg_push_tabs(CgContext* c);

void cg(CgContext* c, char* finoutpath) {
    c->gencode = null;
    c->indent = 0;

    buf_loop(c->srcfile->stmts, i) {
        cg_stmt(c, c->srcfile->stmts[i]);
    }
    buf_push(c->gencode, '\0');
    /* fputs(c->gencode, stdout); */

    char* generic_outpath =
                aria_basename(
                    aria_notdir(c->srcfile->handle->path));
    char* path_dir = aria_dir(finoutpath);
    if (path_dir) {
        generic_outpath = aria_strapp(path_dir, generic_outpath);
    }

    char* iroutpath = aria_strapp(generic_outpath, ".ll");
    char* asmoutpath = aria_strapp(generic_outpath, ".s");
    c->outpath = asmoutpath;

    FILE* file = fopen(iroutpath, "w");
    fwrite(c->gencode, buf_len(c->gencode)-1, sizeof(char), file);
    fclose(file);
    
    char* llc_build_cmd = null;
    buf_printf(llc_build_cmd, "llc %s", iroutpath);
    printf("[llc] %s\n", llc_build_cmd);
    system(llc_build_cmd);
}

void cg_output_bin(CgContext* c, char* finoutpath) {
    char* build_cmd = null;
    buf_printf(build_cmd, "gcc -no-pie -o %s ", finoutpath);
    buf_loop(c, i) {
        buf_printf(build_cmd, "%s ", c[i].outpath);
    }
    buf_printf(build_cmd, "examples/std.c");
    printf("[build] %s\n", build_cmd);
    system(build_cmd);
}

void cg_stmt(CgContext* c, Stmt* stmt) {
    switch (stmt->kind) {
        case STMT_KIND_FUNCTION: {
            cg_function_stmt(c, stmt);
        } break;

    /*     case STMT_KIND_VARIABLE: { */
    /*         cg_variable_stmt(c, stmt); */
    /*     } break; */

    /*     case STMT_KIND_WHILE: { */
    /*         cg_while_stmt(c, stmt); */
    /*     } break; */

    /*     case STMT_KIND_ASSIGN: { */
    /*         cg_assign_stmt(c, stmt); */
    /*     } break; */

    /*     case STMT_KIND_EXPR: { */
    /*         cg_expr_stmt(c, stmt); */
    /*     } break; */
    }
}

void cg_function_stmt(CgContext* c, Stmt* stmt) {
    if (stmt->function.is_extern) {
        /* cg_p(c, "declare %s", stmt->function.header->identifier->lexeme); */
    } 
    else {
        cg_p(c, "define ");
        cg_type(c, stmt->function.header->return_type);
        cg_p(c, "@%s(", stmt->function.header->identifier->lexeme);
		Stmt** params = stmt->function.header->params;
		for (size_t i = 0; i < buf_len(params); i++) {
			cg_type(c, params[i]->param.type);
			cg_p(c, " %%%s", params[i]->param.identifier->lexeme);
			if (i != buf_len(params)-1) cg_p(c, ", ");
		}
		cg_pn(c, ") {");
		cg_pn(c, "ret void");
		cg_pn(c, "}");
    }
}

void cg_type(CgContext* c, Type* type) {
	if (type->kind == TYPE_KIND_BUILTIN) {
		switch (type->builtin.kind) {
			case BUILTIN_TYPE_KIND_U8:
				type = builtin_type_placeholders.i8;
				break;
			case BUILTIN_TYPE_KIND_U16:
				type = builtin_type_placeholders.i16;
				break;
			case BUILTIN_TYPE_KIND_U32:
				type = builtin_type_placeholders.i32;
				break;
			case BUILTIN_TYPE_KIND_U64:
				type = builtin_type_placeholders.i64;
				break;				
		}
	}
    buf_printf_type(c->gencode, type);
    buf_push(c->gencode, ' ');
}

void cg_p(CgContext* c, char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    buf_vprintf(c->gencode, fmt, ap);
    va_end(ap);
}

// newline at end
void cg_pn(CgContext* c, char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    buf_vprintf(c->gencode, fmt, ap);
    buf_push(c->gencode, '\n');
    va_end(ap);
}

void cg_push_tabs(CgContext* c) {
    for (int i = 0; i < c->indent; i++) {
        for (int j = 0; j < TAB_SIZE; j++) {
            buf_push(c->gencode, ' ');
        }
    }
}
