#include "lex.h"

LexCtx lexctx_new(
    struct Srcfile* srcfile,
    struct CompileCtx* compilectx, 
    jmp_buf* error_handler_pos
) {
    LexCtx l;
    l.srcfile = srcfile;
    l.srcfile->tokens = NULL;
    l.start = srcfile->handle.contents;
    l.current = l.start;
    l.lastnl = l.start;
    l.error = false;
    l.compilectx = compilectx;
    l.error_handler_pos = error_handler_pos;
    memset(l.ascii_error_table, 0, 128);
    l.invalid_char_error = false;
    return l;
}

static inline void msg_emit(LexCtx* l, Msg* msg) {
    _msg_emit(msg, l->compilectx);
    if (msg->kind == MSG_ERROR) l->error = true;
}

static inline void fatal_msg_emit(LexCtx* l, Msg* msg) {
    _msg_emit(msg, l->compilectx);
    if (msg->kind == MSG_ERROR) {
        l->error = true;
        longjmp(*l->error_pos, 1);
    }
}

static inline Span span_from_start_to_current(LexCtx* l) {
    return span_new(
        l->srcfile,
        l->start - l->srcfile->handle.contents,
        l->current - l->srcfile->handle.contents
    );
}

static inline Span span_to_current_from(LexCtx* l, const char* from) {
    return span_new(
        l->srcfile,
        from - l->srcfile->handle.contents,
        l->current - l->srcfile->handle.contents
    );
}

static bool match(LexCtx* l, char c) {
    if (*l->current == c) {
        l->current++;
        return true;
    }
    return false;
}

static inline char peek(LexCtx* l) {
    return *(l->current+1); 
}

static void push_tok(LexCtx* l, TokenKind kind) {
    Token* t = token_new(kind, span_from_start_to_current(l));
    bufpush(l->srcfile->tokens, t);
}

static inline void push_tok_adv(LexCtx* l, TokenKind kind) {   
    l->current++;
    push_tok(l, kind);
}

static void push_tok_adv_cond(
    LexCtx* l, 
    char c, 
    TokenKind match, 
    TokenKind orelse
) {
    l->current++;
    if (*l->current == c) push_tok_adv(l, match);
    else push_tok(l, orelse);
}

static inline Token* last_tok(LexCtx* l) {
    return l->srcfile->tokens[buflen(l->srcfile->tokens)-1];
}

void lex(LexCtx* l);
