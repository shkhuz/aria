#include "compile.h"
#include "cmd.h"
#include "core.h"
#include "buf.h"
#include "lex.h"
#include "ast.h"
#include "parse.h"
#include "ast_print.h"

StringKwKind* keywords = NULL;

void init_global_compiler_state() {
    init_cmd();
    bufpush(keywords, (StringKwKind){ "imm", TOKEN_KEYWORD_IMM });
    bufpush(keywords, (StringKwKind){ "mut", TOKEN_KEYWORD_MUT });
    bufpush(keywords, (StringKwKind){ "fn", TOKEN_KEYWORD_FN });
    bufpush(keywords, (StringKwKind){ "type", TOKEN_KEYWORD_TYPE });
    bufpush(keywords, (StringKwKind){ "struct", TOKEN_KEYWORD_STRUCT });
    bufpush(keywords, (StringKwKind){ "if", TOKEN_KEYWORD_IF });
    bufpush(keywords, (StringKwKind){ "else", TOKEN_KEYWORD_ELSE });
    bufpush(keywords, (StringKwKind){ "for", TOKEN_KEYWORD_FOR });
    bufpush(keywords, (StringKwKind){ "return", TOKEN_KEYWORD_RETURN });
    bufpush(keywords, (StringKwKind){ "yield", TOKEN_KEYWORD_YIELD });
}

CompileCtx compile_new_context() {
    CompileCtx c;
    c.srcfiles = NULL;
    c.num_srcfiles = 0;
    c.msgs = NULL;
    c.parsing_error = false;
    c.print_msg_to_stderr = true;
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
            ast_print(p.srcfile->astnodes);
        } else {
            c->parsing_error = true;
            break;
        }
    }
}