#include "compile.h"
#include "lex.h"
#include "parse.h"
#include "dbg.h"

CompileCtx _compilectx_new() {
    CompileCtx c;
    c.msgs = NULL;
    c.print_msg_to_stderr = true;
    c.did_msg = false;
    c.parsing_error = false;
    return c;
}

CompileCtx compilectx_from_stream(const char* stream) {
    CompileCtx c = _compilectx_new();
    c.srcfile = (Srcfile){
        .id = c.next_srcfile_id++,
        .handle = (File){
            .path = "<stream>", 
            .abs_path = "<stream>", 
            .contents = stream,
            .len = strlen(stream)
        },
        .tokens = NULL,
        .ast = NULL,
    };
    return c;
}

void compile(CompileCtx* c) {
    jmp_buf lex_error_handler_pos;
    jmp_buf parse_error_handler_pos;

    LexCtx l = lexctx_new(&c->srcfile, c, &lex_error_handler_pos);
    if (!setjmp(lex_error_handler_pos)) {
        lex(&l);
        if (l.error) {
            c->parsing_error = true;
            //continue;
            return;
        } else dbg_print_tokens(l.srcfile->tokens);
    } 
    else {
        c->parsing_error = true;
        //continue;
        return;
    }

    ParseCtx p = parsectx_new(&c->srcfile, c, &parse_error_handler_pos);
    if (!setjmp(parse_error_handler_pos)) {
        parse(&p);
        if (p.error) c->parsing_error = true;
        else dbg_print_ast(c->srcfile.ast);
        // else if (c->print_ast) ast_print(c->srcfile.astnodes);
    }
    else c->parsing_error = true;
    
    if (c->parsing_error) return;
}

void compile_register_msg(CompileCtx* c, Msg msg) {
    bufpush(c->msgs, msg);
}

void compile_terminate(CompileCtx* c) {
    exit(1);
}
