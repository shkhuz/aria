#include "compile.h"
#include "lex.h"
#include "parse.h"
#include "dbg.h"

// TODO: make these fixed length (static allocation)
StrTokenMap* keywords;
StrTokenMap* directives;

CompileCtx compilectx_new() {
    CompileCtx c = (CompileCtx){};
    c.permarena = arena_create(4ULL << 30);
    if (!c.permarena.base) {
        fprintf(stderr, "error: cannot allocate `permarena`\n");
        compile_terminate(&c);
    }

    // c.fendarena = arena_create(4ULL << 30);
    // if (!c.fendarena.base) {
    //     fprintf(stderr, "error: cannot allocate `fendarena`\n");
    //     compile_terminate(&c);
    // }

    c.msgs = NULL;
    c.print_msg_to_stderr = true;
    c.did_msg = false;
    listinit(&c.permarena, c.srcfiles);
    list_debug_dump_uniform_chunks(c.srcfiles);
    c.parsing_error = false;
    // stri_init(&c.permarena, &c.interner);

    // listinit(&c.fendarena, keywords);
    // listinit(&c.fendarena, directives);

#define DEF(list, k, v) (listpush(\
    list,\
    (StrTokenMap){stri_intern(&c.interner, k, strlen(k)), v}\
));
    // DEF(keywords,   "comp",         TK_KW_COMP);
    // DEF(keywords,   "imm",          TK_KW_IMM);
    // DEF(keywords,   "mut",          TK_KW_MUT);
    // DEF(keywords,   "fn",           TK_KW_FN);
    // DEF(keywords,   "struct",       TK_KW_STRUCT);
    // DEF(keywords,   "yield",        TK_KW_YIELD);

    // DEF(directives, "cast",         TK_DT_CAST);
#undef DEF

    return c;
}

int read_srcfile(
    CompileCtx* c, 
    const char* path, 
    Span span,
    Srcfile* spansrc
) {
    FileOrError efile = read_file(&c->permarena, path);
    switch (efile.status) {
        case FILEIO_SUCCESS: {
            for (usize i = 0; i < listlen(c->srcfiles); i++) {
                if (strcmp(
                    efile.handle.abs_path, 
                    listget(c->srcfiles, i)->handle.abs_path
                ) == 0) {
                    return (int)i;
                }
            }

            Srcfile* src = ALLOC_OBJ(&c->permarena, Srcfile);
            src->handle = efile.handle,
            src->tokens = NULL,
            src->nodes = NULL,
            src->nextra = NULL,
            listpush(c->srcfiles, src);
            return listlastidx(c->srcfiles);
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
    Srcfile* psrc = ALLOC_OBJ(&c->permarena, Srcfile);
    *psrc = src;
    listpush(c->srcfiles, psrc);
    return listlastidx(c->srcfiles);
}

int compilectx_init_path(CompileCtx* c, const char* path) {
    return read_srcfile(c, path, (Span){}, NULL);
}

void compile(CompileCtx* c) {
    jmp_buf lex_error_handler_pos;
    jmp_buf parse_error_handler_pos;

    usize source_mem = 0;
    usize tokens_count = 0, tokens_mem = 0;
    usize nodes_count = 0, nodes_mem = 0;
    usize nextra_count = 0, nextra_mem = 0;
    usize total = 0;
    for (usize i = 0; i < listlen(c->srcfiles); i++) {
        Srcfile* src = listget(c->srcfiles, i);
        source_mem += src->handle.len;
        printf("\nCompiling %s", src->handle.path);
        // LexCtx l = lexctx_new(src, c, &lex_error_handler_pos);
        // if (!setjmp(lex_error_handler_pos)) {
        //     lex(&l);
        //     if (l.error) {
        //         c->parsing_error = true;
        //         continue;
        //     } 
        //     // else dbg_print_tokens(src);
        //     tokens_count += listlen(src->tokens);
        //     tokens_mem += listcap(src->tokens)*sizeof(Token);
        // } else {
        //     c->parsing_error = true;
        //     continue;
        // }

        // ParseCtx p = parsectx_new(
        //     src,
        //     c, 
        //     &parse_error_handler_pos
        // );
        // if (!setjmp(parse_error_handler_pos)) {
        //     parse(&p);
        //     if (p.error) {
        //         c->parsing_error = true;
        //         continue;
        //     }
        //     // else dbg_nodes(&p);
        //     nodes_count += listlen(src->nodes);
        //     nodes_mem += listcap(src->nodes)*sizeof(Node);
        //     nextra_count += listlen(src->nextra);
        //     nextra_mem += listcap(src->nextra)*sizeof(int);
        //     nextra_mem += listcap(p.sextra)*sizeof(int);
        // } else {
        //     c->parsing_error = true;
        //     continue;
        // }
    }

    printf("\nSource: ");
    total += print_memory_size(source_mem);

    printf("\nKeywords (%lu): ", listlen(keywords));
    total += print_memory_size(listcap(keywords)*sizeof(StrTokenMap));

    printf("\nDirectives (%lu): ", listlen(directives));
    total += print_memory_size(listcap(directives)*sizeof(StrTokenMap));

    printf("\nTokens (%lu): ", tokens_count);
    total += print_memory_size(tokens_mem);
    
    printf("\nNodes (%lu): ", nodes_count);
    total += print_memory_size(nodes_mem);
    
    printf("\nNextra (%lu): ", nextra_count);
    total += print_memory_size(nextra_mem);

    printf("\nStri:");
    printf("\n  slices (%lu): ", listlen(c->interner.slices));
    total += print_memory_size(listcap(c->interner.slices)*sizeof(strislice));
    printf("\n  buckets (%lu): ", listlen(c->interner.buckets));
    total += print_memory_size(listcap(c->interner.buckets)*sizeof(u32));
    printf("\n  nodes (%lu): ", listlen(c->interner.nodes));
    total += print_memory_size(listcap(c->interner.nodes)*sizeof(strinode));
    printf("\nTotal: %lu or ", total);
    print_memory_size(total);

    printf("\nPermarena Mem: ");
    print_memory_size(c->permarena.pos);
    
    printf("\nFront-end Mem: ");
    print_memory_size(c->fendarena.pos);

    if (c->parsing_error) return;
}

void compile_register_msg(CompileCtx* c, Msg msg) {
    bufpush(c->msgs, msg);
}

void compile_terminate(CompileCtx* c) {
    exit(1);
}
