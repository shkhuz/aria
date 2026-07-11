#include "compile.h"
#include "lex.h"
#include "parse.h"
#include "dbg.h"

CompileCtx compilectx_new() {
    CompileCtx c;
    c.msgs = NULL;
    c.print_msg_to_stderr = true;
    c.did_msg = false;
    c.srcfiles = NULL;
    c.parsing_error = false;
    return c;
}

Srcfile* read_srcfile(
    CompileCtx* c, 
    const char* path, 
    OptionalSpan span
) {
    FileOrError efile = read_file(path);
    switch (efile.status) {
        case FILEIO_SUCCESS: {
            for (usize i = 0; i < buflen(c->srcfiles); i++) {
                if (strcmp(
                    efile.handle.abs_path, 
                    c->srcfiles[i].handle.abs_path
                ) == 0) {
                    return &c->srcfiles[i];
                }
            }

            Srcfile src = (Srcfile){
                .id = c->next_srcfile_id++,
                .handle = efile.handle,
                .tokens = NULL,
                .ast = NULL,
            };
            bufpush(c->srcfiles, src);
            return buflast(c->srcfiles);
        } break;

        case FILEIO_DIRECTORY:
        case FILEIO_FAILURE: {
            const char* error_msg = format_string(
                "cannot read file '%s'",
                path
            );
            if (span.exists) {
                Msg msg = msg_with_span(
                    MSG_ERROR,
                    error_msg,
                    span.span
                );
                _msg_emit(&msg, c);
            } else {
                Msg msg = msg_with_no_span(
                    MSG_ERROR,
                    error_msg
                );
                _msg_emit(&msg, c);
            }
        } break;
    }
}

void compilectx_init_stream(CompileCtx* c, const char* stream) {
    bufpush(c->srcfiles, (Srcfile){
        .id = c->next_srcfile_id++,
        .handle = (File){
            .path = "<stream>", 
            .abs_path = "<stream>", 
            .contents = stream,
            .len = strlen(stream)
        },
        .tokens = NULL,
        .ast = NULL,
    });
}

bool compilectx_init_path(
    CompileCtx* c, 
    const char* path, 
    OptionalSpan span
) {
    Srcfile* src = read_srcfile(c, path, span);
    return src != NULL;
}

void compile(CompileCtx* c) {
    jmp_buf lex_error_handler_pos;
    jmp_buf parse_error_handler_pos;

    for (usize i = 0; i < buflen(c->srcfiles); i++) {
        printf("\n:: Compiling %s", c->srcfiles[i].handle.path);
        LexCtx l = lexctx_new(
            &c->srcfiles[i], 
            c, 
            &lex_error_handler_pos
        );
        if (!setjmp(lex_error_handler_pos)) {
            lex(&l);
            if (l.error) {
                c->parsing_error = true;
                continue;
            } 
            // else dbg_print_tokens(l.srcfile->tokens);
        } else {
            c->parsing_error = true;
            continue;
        }

        ParseCtx p = parsectx_new(
            &c->srcfiles[i], 
            c, 
            &parse_error_handler_pos
        );
        if (!setjmp(parse_error_handler_pos)) {
            parse(&p);
            if (p.error) c->parsing_error = true;
            else /*if (c->print_ast)*/ dbg_print_ast(c->srcfiles[i].ast);
        } else {
            c->parsing_error = true;
            continue;
        }
    }
    
    if (c->parsing_error) return;
}

void compile_register_msg(CompileCtx* c, Msg msg) {
    bufpush(c->msgs, msg);
}

void compile_terminate(CompileCtx* c) {
    exit(1);
}
