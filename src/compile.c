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

void init_global_compiler_state() {
    init_cmd();

    bufpush(keywords, (StringTokenKindTup){ "imm", TOKEN_KEYWORD_IMM });
    bufpush(keywords, (StringTokenKindTup){ "mut", TOKEN_KEYWORD_MUT });
    bufpush(keywords, (StringTokenKindTup){ "pub", TOKEN_KEYWORD_PUB });
    bufpush(keywords, (StringTokenKindTup){ "fn", TOKEN_KEYWORD_FN });
    bufpush(keywords, (StringTokenKindTup){ "type", TOKEN_KEYWORD_TYPE });
    bufpush(keywords, (StringTokenKindTup){ "struct", TOKEN_KEYWORD_STRUCT });
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
    bufpush(keywords, (StringTokenKindTup){ "as", TOKEN_KEYWORD_AS });
    bufpush(keywords, (StringTokenKindTup){ "and", TOKEN_KEYWORD_AND });
    bufpush(keywords, (StringTokenKindTup){ "or", TOKEN_KEYWORD_OR });

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
        .void_type = typespec_type_new(typespec_prim_new(PRIM_void)),
        .noreturn_type = typespec_type_new(typespec_noreturn_new()),
    };
}

CompileCtx compile_new_context(const char* target_triple) {
    CompileCtx c;
    c.mod_tys = NULL;
    c.target_triple = target_triple;
    c.msgs = NULL;
    c.parsing_error = false;
    c.sema_error = false;
    c.cg_error = false;
    c.print_msg_to_stderr = true;
    c.print_ast = false;
    c.did_msg = false;
    return c;
}

void register_msg(CompileCtx* c, Msg msg) {
    bufpush(c->msgs, msg);
}

void compile(CompileCtx* c) {
    jmp_buf parse_error_handler_pos;

    for (usize i = 0; i < buflen(c->mod_tys); i++) {
        LexCtx l = lex_new_context(c->mod_tys[i]->mod.srcfile, c);
        lex(&l);
        if (l.error) {
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
}

struct Typespec* read_srcfile(const char* path, OptionalSpan span, CompileCtx* compile_ctx) {
    FileOrError efile = read_file(path);
    switch (efile.status) {
        case FOO_SUCCESS: {
            for (usize i = 0; i < buflen(compile_ctx->mod_tys); i++) {
                if (strcmp(efile.handle.abs_path, compile_ctx->mod_tys[i]->mod.srcfile->handle.abs_path) == 0)
                    return compile_ctx->mod_tys[i];
            }
            Srcfile* srcfile = malloc(sizeof(Srcfile));
            srcfile->handle = efile.handle;
            Typespec* mod = typespec_module_new(srcfile);
            bufpush(compile_ctx->mod_tys, mod);
            return mod;
        } break;

        case FOO_FAILURE: {
            const char* error_msg = format_string("cannot read source file '%s'", path);
            if (span.exists) {
                Msg msg = msg_with_span(MSG_ERROR, error_msg, span.span);
                _msg_emit(&msg, compile_ctx);
            } else {
                Msg msg = msg_with_no_span(MSG_ERROR, error_msg);
                _msg_emit(&msg, compile_ctx);
            }
        } break;

        case FOO_DIRECTORY: {
            const char* error_msg = format_string("`%s` is a directory", path);
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
