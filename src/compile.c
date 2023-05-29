#include "compile.h"
#include "cmd.h"
#include "core.h"
#include "buf.h"
#include "lex.h"
#include "ast.h"
#include "parse.h"
#include "ast_print.h"
#include "sema.h"
#include "type.h"

StringTokenKindTup* keywords = NULL;
StringDirectiveKindTup* directives = NULL;
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
    bufpush(keywords, (StringTokenKindTup){ "for", TOKEN_KEYWORD_FOR });
    bufpush(keywords, (StringTokenKindTup){ "return", TOKEN_KEYWORD_RETURN });
    bufpush(keywords, (StringTokenKindTup){ "yield", TOKEN_KEYWORD_YIELD });

    bufpush(directives, (StringDirectiveKindTup){ "import", DIRECTIVE_IMPORT });

    predef_typespecs = (PredefTypespecs){
        .u8_type = typespec_type_new(typespec_prim_new(PRIM_u8)),
        .u16_type = typespec_type_new(typespec_prim_new(PRIM_u16)),
        .u32_type = typespec_type_new(typespec_prim_new(PRIM_u32)),
        .u64_type = typespec_type_new(typespec_prim_new(PRIM_u64)),
        .i8_type = typespec_type_new(typespec_prim_new(PRIM_i8)),
        .i16_type = typespec_type_new(typespec_prim_new(PRIM_i16)),
        .i32_type = typespec_type_new(typespec_prim_new(PRIM_i32)),
        .i64_type = typespec_type_new(typespec_prim_new(PRIM_i64)),
        .void_type = typespec_type_new(typespec_prim_new(PRIM_void)),
    };
}

CompileCtx compile_new_context() {
    CompileCtx c;
    c.srcfiles = NULL;
    c.num_srcfiles = 0;
    c.msgs = NULL;
    c.parsing_error = false;
    c.sema_error = false;
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

    for (usize i = 0; i < c->num_srcfiles; i++) {
        LexCtx l = lex_new_context(c->srcfiles[i], c);
        lex(&l);
        if (l.error) {
            c->parsing_error = true;
            continue;
        }

        ParseCtx p = parse_new_context(
            c->srcfiles[i],
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

    if (c->parsing_error) return;
    SemaCtx* sema_ctxs = NULL;
    for (usize i = 0; i < c->num_srcfiles; i++) {
        bufpush(sema_ctxs, sema_new_context(c->srcfiles[i], c));
    }

    c->sema_error = sema(sema_ctxs);
}
