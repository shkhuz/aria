#include "compile.h"
#include "cmd.h"
#include "core.h"
#include "buf.h"
#include "lex.h"
#include "ast.h"
#include "parse.h"
#include "ast_print.h"
#include "sema.h"
#include "cg.h"
#include "type.h"

StringTokenKindTup* keywords = NULL;
StringBuiltinSymbolKindTup* builtin_symbols = NULL;
PredefTypespecs predef_typespecs;

char g_exec_path[PATH_MAX+1];
char* g_lib_path;

void init_global_compiler_state() {
    init_cmd();

    bufpush(keywords, (StringTokenKindTup){ "imm", TOKEN_KEYWORD_IMM });
    bufpush(keywords, (StringTokenKindTup){ "mut", TOKEN_KEYWORD_MUT });
    bufpush(keywords, (StringTokenKindTup){ "pub", TOKEN_KEYWORD_PUB });
    bufpush(keywords, (StringTokenKindTup){ "fn", TOKEN_KEYWORD_FN });
    bufpush(keywords, (StringTokenKindTup){ "type", TOKEN_KEYWORD_TYPE });
    bufpush(keywords, (StringTokenKindTup){ "struct", TOKEN_KEYWORD_STRUCT });
    bufpush(keywords, (StringTokenKindTup){ "packed", TOKEN_KEYWORD_PACKED });
    bufpush(keywords, (StringTokenKindTup){ "impl", TOKEN_KEYWORD_IMPL });
    bufpush(keywords, (StringTokenKindTup){ "if", TOKEN_KEYWORD_IF });
    bufpush(keywords, (StringTokenKindTup){ "else", TOKEN_KEYWORD_ELSE });
    bufpush(keywords, (StringTokenKindTup){ "while", TOKEN_KEYWORD_WHILE });
    bufpush(keywords, (StringTokenKindTup){ "for", TOKEN_KEYWORD_FOR });
    bufpush(keywords, (StringTokenKindTup){ "break", TOKEN_KEYWORD_BREAK });
    bufpush(keywords, (StringTokenKindTup){ "continue", TOKEN_KEYWORD_CONTINUE });
    bufpush(keywords, (StringTokenKindTup){ "return", TOKEN_KEYWORD_RETURN });
    bufpush(keywords, (StringTokenKindTup){ "yield", TOKEN_KEYWORD_YIELD });
    bufpush(keywords, (StringTokenKindTup){ "import", TOKEN_KEYWORD_IMPORT });
    bufpush(keywords, (StringTokenKindTup){ "export", TOKEN_KEYWORD_EXPORT });
    bufpush(keywords, (StringTokenKindTup){ "extern", TOKEN_KEYWORD_EXTERN });
    bufpush(keywords, (StringTokenKindTup){ "as", TOKEN_KEYWORD_AS });
    bufpush(keywords, (StringTokenKindTup){ "and", TOKEN_KEYWORD_AND });
    bufpush(keywords, (StringTokenKindTup){ "or", TOKEN_KEYWORD_OR });
    bufpush(keywords, (StringTokenKindTup){ "_", TOKEN_KEYWORD_UNDERSCORE });

    bufpush(builtin_symbols, (StringBuiltinSymbolKindTup){ "u8", BS_u8 });
    bufpush(builtin_symbols, (StringBuiltinSymbolKindTup){ "u16", BS_u16 });
    bufpush(builtin_symbols, (StringBuiltinSymbolKindTup){ "u32", BS_u32 });
    bufpush(builtin_symbols, (StringBuiltinSymbolKindTup){ "u64", BS_u64 });
    bufpush(builtin_symbols, (StringBuiltinSymbolKindTup){ "i8", BS_i8 });
    bufpush(builtin_symbols, (StringBuiltinSymbolKindTup){ "i16", BS_i16 });
    bufpush(builtin_symbols, (StringBuiltinSymbolKindTup){ "i32", BS_i32 });
    bufpush(builtin_symbols, (StringBuiltinSymbolKindTup){ "i64", BS_i64 });
    bufpush(builtin_symbols, (StringBuiltinSymbolKindTup){ "bool", BS_bool });
    bufpush(builtin_symbols, (StringBuiltinSymbolKindTup){ "void", BS_void });
    bufpush(builtin_symbols, (StringBuiltinSymbolKindTup){ "noreturn", BS_noreturn });
    bufpush(builtin_symbols, (StringBuiltinSymbolKindTup){ "true", BS_true });
    bufpush(builtin_symbols, (StringBuiltinSymbolKindTup){ "false", BS_false });

    predef_typespecs = (PredefTypespecs){
        .u8_type = typespec_type_new(typespec_prim_new(PRIM_u8)),
        .u16_type = typespec_type_new(typespec_prim_new(PRIM_u16)),
        .u32_type = typespec_type_new(typespec_prim_new(PRIM_u32)),
        .u64_type = typespec_type_new(typespec_prim_new(PRIM_u64)),
        .i8_type = typespec_type_new(typespec_prim_new(PRIM_i8)),
        .i16_type = typespec_type_new(typespec_prim_new(PRIM_i16)),
        .i32_type = typespec_type_new(typespec_prim_new(PRIM_i32)),
        .i64_type = typespec_type_new(typespec_prim_new(PRIM_i64)),
        .bool_type = typespec_type_new(typespec_prim_new(PRIM_bool)),
        .void_type = typespec_type_new(typespec_void_new()),
        .noreturn_type = typespec_type_new(typespec_noreturn_new()),
    };

    {
        ssize_t len = readlink("/proc/self/exe", g_exec_path, PATH_MAX);
        if (len == -1) {
            fprintf(stderr, "readlink(): cannot get executable path");
            exit(1);
        }
        g_exec_path[len] = '\0';
        g_lib_path = NULL;
        bufstrexpandpush(g_lib_path, g_exec_path);
        // Remove executable name
        while (g_lib_path[buflen(g_lib_path)-1] != '/') bufpop(g_lib_path);
        bufpop(g_lib_path);
        //  Remove parent directory
        while (g_lib_path[buflen(g_lib_path)-1] != '/') bufpop(g_lib_path);
        bufstrexpandpush(g_lib_path, "lib/aria/");
        bufpush(g_lib_path, '\0');
    }
}

Typespec* get_predef_integer_type(int bytes, bool signd) {
    if (signd) {
        switch (bytes) {
            case 1: return predef_typespecs.i8_type;
            case 2: return predef_typespecs.i16_type;
            case 4: return predef_typespecs.i32_type;
            case 8: return predef_typespecs.i64_type;
        }
    } else {
        switch (bytes) {
            case 1: return predef_typespecs.u8_type;
            case 2: return predef_typespecs.u16_type;
            case 4: return predef_typespecs.u32_type;
            case 8: return predef_typespecs.u64_type;
        }
    }
    assert(0);
    return NULL;
}

CompileCtx compile_new_context(
        const char* outpath,
        const char* target_triple,
        bool naked) {
    CompileCtx c;
    c.mod_tys = NULL;
    c.outpath = outpath;
    c.target_triple = target_triple;
    c.naked = naked;
    c.other_obj_files = NULL;
    c.msgs = NULL;
    c.parsing_error = false;
    c.sema_error = false;
    c.cg_error = false;
    c.compile_error = false;
    c.print_msg_to_stderr = true;
    c.print_ast = false;
    c.did_msg = false;
    c.next_srcfile_id = 0;
    return c;
}

void register_msg(CompileCtx* c, Msg msg) {
    bufpush(c->msgs, msg);
}

static void msg_emit(CompileCtx* c, Msg* msg) {
    _msg_emit(msg, c);
    if (msg->kind == MSG_ERROR) {
        c->compile_error = true;
    }
}

static char* flatten_execopts(char** opts) {
    char* str = NULL;
    // buflen()-1 because last element is NULL
    for (usize i = 0; i < buflen(opts)-1; i++) {
        bufstrexpandpush(str, opts[i]);
        if (i != buflen(opts)-2) {
            bufpush(str, ' ');
        }
    }
    bufpush(str, '\0');
    return str;
}

static bool run_external_program(CompileCtx* c, char* path, char** opts, char* desc) {
    int errdesc[2];
    if (pipe2(errdesc, O_CLOEXEC) == -1) {
        fprintf(stderr, "cannot execute pipe2(): %s", strerror(errno));
        c->compile_error = true; return false;
    }

    pid_t proc = fork();
    if (proc == -1) {
        fprintf(stderr, "cannot execute fork(): %s", strerror(errno));
        c->compile_error = true; return false;
    }

    if (proc) {
        close(errdesc[1]);
        char buf[2];
        int status;
        wait(&status);

        int bytesread = read(errdesc[0], buf, sizeof(char));
        if (bytesread == 0 && WEXITSTATUS(status) != 0) {
            Msg msg = msg_with_no_span(
                MSG_ERROR,
                "aborting due to previous error");
            msg_addl_thin(&msg, format_string("command executed: '%s'", flatten_execopts(opts)));
            msg_emit(c, &msg);
            return false;
        }
    } else {
        if (execvp(path, opts) == -1) {
            close(errdesc[0]);
            write(errdesc[1], "F", sizeof(char));
            close(errdesc[1]);
        }
        Msg msg = msg_with_no_span(
            MSG_ERROR,
            format_string("%s '%s' not found", desc ? desc : "program", path));
        msg_emit(c, &msg);
        return false;
    }
    return true;
}

void compile(CompileCtx* c) {
    jmp_buf lex_error_handler_pos;
    jmp_buf parse_error_handler_pos;

    for (usize i = 0; i < buflen(c->mod_tys); i++) {
        LexCtx l = lex_new_context(c->mod_tys[i]->mod.srcfile, c, &lex_error_handler_pos);
        if (!setjmp(lex_error_handler_pos)) {
            lex(&l);
            if (l.error) {
                c->parsing_error = true;
                continue;
            }
        } else {
            c->parsing_error = true;
            continue;
        }

        ParseCtx p = parse_new_context(
            c->mod_tys[i]->mod.srcfile,
            c,
            &parse_error_handler_pos);
        if (!setjmp(parse_error_handler_pos)) {
            parse(&p);
            if (p.error) c->parsing_error = true;
            else if (c->print_ast) ast_print(p.srcfile->astnodes);
        } else {
            c->parsing_error = true;
            continue;
        }
    }
    if (c->print_ast) printf("\n");

    if (c->parsing_error) return;
    SemaCtx* sema_ctxs = NULL;
    for (usize i = 0; i < buflen(c->mod_tys); i++) {
        bufpush(sema_ctxs, sema_new_context(c->mod_tys[i]->mod.srcfile, c));
    }

    c->sema_error = sema(sema_ctxs);
    if (c->sema_error) return;

    CgCtx cg_ctx = cg_new_context(c->mod_tys, c);
    c->cg_error = cg(&cg_ctx);
    if (c->cg_error) return;

    if (!c->naked) {
#if defined(PLATFORM_AMD64)
        const char* startfile = "_start-x86_64.S";
#elif defined(PLATFORM_AARCH64)
        const char* startfile = "_start-aarch64.S";
#else
        Msg msg = msg_with_no_span(
            MSG_ERROR,
            "unknown platform: no startfile found");
        _msg_emit(&msg, c);
        c->cg_error = true;
        return;
#endif

        char* startfile_path = NULL;
        bufstrexpandpush(startfile_path, g_lib_path);
        bufstrexpandpush(startfile_path, startfile);
        bufpush(startfile_path, '\0');
        if (!file_exists(startfile_path)) {
            Msg msg = msg_with_no_span(
                MSG_ERROR,
                format_string("startfile '%s' not found", startfile_path));
            msg_emit(c, &msg);
            return;
        }

        char** asopts = NULL;
        bufpush(asopts, "as");
        bufpush(asopts, "-g");
        bufpush(asopts, "-o");
        bufpush(asopts, "_start.o");
        bufpush(asopts, startfile_path);
        bufpush(asopts, NULL);
        if (!run_external_program(c, "as", asopts, "assembler")) return;
        buffree(asopts);

        char** ldopts = NULL;
        bufpush(ldopts, "ld");
        if (c->outpath) {
            bufpush(ldopts, "-o");
            bufpush(ldopts, (char*)c->outpath);
        }
        bufpush(ldopts, "mod.o");
        bufpush(ldopts, "_start.o");
        bufloop(c->other_obj_files, i) {
            bufpush(ldopts, c->other_obj_files[i]);
        }
        bufpush(ldopts, NULL);
        if (!run_external_program(c, "ld", ldopts, "linker")) return;
        buffree(ldopts);
    }

    char** rmopts = NULL;
    bufpush(rmopts, "rm");
    bufpush(rmopts, "-f");
    if (!c->naked || (c->outpath && strcmp(c->outpath, "mod.o") != 0)) bufpush(rmopts, "mod.o");
    bufpush(rmopts, "_start.o");
    bufpush(rmopts, NULL);
    if (!run_external_program(c, "rm", rmopts, NULL)) return;
    buffree(rmopts);
}

struct Typespec* read_srcfile(char* path_wcwd, const char* path_wfile, OptionalSpan span, CompileCtx* compile_ctx) {
    char* final_path = NULL;
    bool readlib = false;
    if (strstr(path_wcwd, ".ar") == NULL && path_wfile) {
        bufstrexpandpush(final_path, g_lib_path);
        bufstrexpandpush(final_path, path_wfile);
        bufstrexpandpush(final_path, ".ar");
        bufpush(final_path, '\0');
        readlib = true;
    } else {
        final_path = path_wcwd;
    }

    FileOrError efile = read_file(final_path);
    switch (efile.status) {
        case FILEIO_SUCCESS: {
            for (usize i = 0; i < buflen(compile_ctx->mod_tys); i++) {
                if (strcmp(efile.handle.abs_path, compile_ctx->mod_tys[i]->mod.srcfile->handle.abs_path) == 0)
                    return compile_ctx->mod_tys[i];
            }
            Srcfile* srcfile = malloc(sizeof(Srcfile));
            srcfile->id = compile_ctx->next_srcfile_id++;
            srcfile->handle = efile.handle;
            Typespec* mod = typespec_module_new(srcfile);
            bufpush(compile_ctx->mod_tys, mod);
            return mod;
        } break;

        case FILEIO_FAILURE: {
            const char* error_msg = format_string(readlib ? "cannot read library file '%s'" : "cannot read source file '%s'", final_path);
            if (span.exists) {
                Msg msg = msg_with_span(MSG_ERROR, error_msg, span.span);
                _msg_emit(&msg, compile_ctx);
            } else {
                Msg msg = msg_with_no_span(MSG_ERROR, error_msg);
                _msg_emit(&msg, compile_ctx);
            }
        } break;

        case FILEIO_DIRECTORY: {
            const char* error_msg = format_string("`%s` is a directory", final_path);
            if (span.exists) {
                Msg msg = msg_with_span(MSG_ERROR, error_msg, span.span);
                _msg_emit(&msg, compile_ctx);
            } else {
                Msg msg = msg_with_no_span(MSG_ERROR, error_msg);
                _msg_emit(&msg, compile_ctx);
            }
        } break;
    }
    return NULL;
}

void terminate_compilation(CompileCtx* c) {
    char** rmopts = NULL;
    bufpush(rmopts, "rm");
    bufpush(rmopts, "-f");
    bufpush(rmopts, "mod.o");
    bufpush(rmopts, "_start.o");
    bufpush(rmopts, "a.out");
    bufpush(rmopts, NULL);
    run_external_program(c, "rm", rmopts, NULL);
    buffree(rmopts);
    exit(1);
}
