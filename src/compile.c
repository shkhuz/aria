#include "compile.h"
#include "lex.h"
#include "parse.h"
#include "dbg.h"

StrTokenMap* keywords;
StrTokenMap* directives;

CompileCtx compilectx_new() {
    CompileCtx c;
    c.msgs = NULL;
    c.print_msg_to_stderr = true;
    c.did_msg = false;
    c.srcfiles = NULL;
    c.parsing_error = false;
    stri_init(&c.interner);

#define DEF(buf, k, v) (bufpush(\
    buf,\
    (StrTokenMap){stri_intern(&c.interner, k, strlen(k)), v}\
));
    DEF(keywords,   "comp",         TK_KW_COMP);
    DEF(keywords,   "imm",          TK_KW_IMM);
    DEF(keywords,   "mut",          TK_KW_MUT);
    DEF(keywords,   "fn",           TK_KW_FN);
    DEF(keywords,   "struct",       TK_KW_STRUCT);
    DEF(keywords,   "yield",        TK_KW_YIELD);

    DEF(directives, "cast",         TK_DT_CAST);
#undef DEF

    return c;
}

int read_srcfile(
    CompileCtx* c, 
    const char* path, 
    Span span,
    Srcfile* spansrc
) {
    FileOrError efile = read_file(path);
    switch (efile.status) {
        case FILEIO_SUCCESS: {
            for (usize i = 0; i < buflen(c->srcfiles); i++) {
                if (strcmp(
                    efile.handle.abs_path, 
                    c->srcfiles[i]->handle.abs_path
                ) == 0) {
                    return (int)i;
                }
            }

            Srcfile* src = ALLOC_OBJ(Srcfile);
            src->handle = efile.handle,
            src->tokens = NULL,
            src->nodes = NULL,
            src->nextra = NULL,
            bufpush(c->srcfiles, src);
            return buflastidx(c->srcfiles);
        } break;

        case FILEIO_DIRECTORY:
        case FILEIO_FAILURE: {
            const char* error_msg = format_string(
                "cannot read file '%s'",
                path
            );
            if (spansrc) {
                Msg msg = _msg_with_span(
                    MSG_ERROR,
                    error_msg,
                    span,
                    spansrc
                );
                _msg_emit(&msg, c);
            } else {
                Msg msg = msg_with_no_span(
                    MSG_ERROR,
                    error_msg
                );
                _msg_emit(&msg, c);
            }
            return -1;
        } break;
    }
    return -1;
}

int compilectx_init_stream(CompileCtx* c, const char* stream) {
    Srcfile src = (Srcfile){
        .handle = (File){
            .path = "<stream>", 
            .abs_path = "<stream>", 
            .contents = stream,
            .len = strlen(stream)
        },
        .tokens = NULL,
        .nodes = NULL,
        .nextra = NULL
    };
    Srcfile* psrc = ALLOC_OBJ(Srcfile);
    *psrc = src;
    bufpush(c->srcfiles, psrc);
    return buflastidx(c->srcfiles);
}

int compilectx_init_path(CompileCtx* c, const char* path) {
    return read_srcfile(c, path, (Span){}, NULL);
}

usize print_memory_size(usize bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = (double)bytes;

    while (size >= 1024 && unit_index < 4) {
        size /= 1024;
        unit_index++;
    }

    if (unit_index == 0) {
        printf("%.0f %s", size, units[unit_index]);
    } else {
        printf("%.2f %s", size, units[unit_index]);
    }
    return bytes;
}

void compile(CompileCtx* c) {
    jmp_buf lex_error_handler_pos;
    jmp_buf parse_error_handler_pos;

    usize source_mem = 0;
    usize tokens_count = 0, tokens_mem = 0;
    usize nodes_count = 0, nodes_mem = 0;
    usize nextra_count = 0, nextra_mem = 0;
    usize total = 0;
    for (usize i = 0; i < buflen(c->srcfiles); i++) {
        Srcfile* src = c->srcfiles[i];
        source_mem += src->handle.len;
        printf("\nCompiling %s", src->handle.path);
        LexCtx l = lexctx_new(src, c, &lex_error_handler_pos);
        if (!setjmp(lex_error_handler_pos)) {
            lex(&l);
            if (l.error) {
                c->parsing_error = true;
                continue;
            } 
            // else dbg_print_tokens(src);
            tokens_count += buflen(src->tokens);
            tokens_mem += bufcap(src->tokens)*sizeof(Token);
        } else {
            c->parsing_error = true;
            continue;
        }

        ParseCtx p = parsectx_new(
            c->srcfiles[i], 
            c, 
            &parse_error_handler_pos
        );
        if (!setjmp(parse_error_handler_pos)) {
            parse(&p);
            if (p.error) {
                c->parsing_error = true;
                continue;
            }
            // else dbg_nodes(&p);
            nodes_count += buflen(src->nodes);
            nodes_mem += bufcap(src->nodes)*sizeof(Node);
            nextra_count += buflen(src->nextra);
            nextra_mem += bufcap(src->nextra)*sizeof(int);
        } else {
            c->parsing_error = true;
            continue;
        }
    }

    printf("\nSource: ");
    total += print_memory_size(source_mem);

    printf("\nKeywords (%lu): ", buflen(keywords));
    total += print_memory_size(bufcap(keywords)*sizeof(StrTokenMap));

    printf("\nDirectives (%lu): ", buflen(directives));
    total += print_memory_size(bufcap(directives)*sizeof(StrTokenMap));

    printf("\nTokens (%lu): ", tokens_count);
    total += print_memory_size(tokens_mem);
    
    printf("\nNodes (%lu): ", nodes_count);
    total += print_memory_size(nodes_mem);
    
    printf("\nNextra (%lu): ", nextra_count);
    total += print_memory_size(nextra_mem);

    printf("\nStri:");
    printf("\n  slices (%lu): ", buflen(c->interner.slices));
    total += print_memory_size(bufcap(c->interner.slices)*sizeof(strislice));
    printf("\n  buckets (%lu): ", buflen(c->interner.buckets));
    total += print_memory_size(bufcap(c->interner.buckets)*sizeof(u32));
    printf("\n  nodes (%lu): ", buflen(c->interner.nodes));
    total += print_memory_size(bufcap(c->interner.nodes)*sizeof(strinode));
    printf("\nTotal: %lu or ", total);
    print_memory_size(total);
    
    if (c->parsing_error) return;
}

void compile_register_msg(CompileCtx* c, Msg msg) {
    bufpush(c->msgs, msg);
}

void compile_terminate(CompileCtx* c) {
    exit(1);
}
