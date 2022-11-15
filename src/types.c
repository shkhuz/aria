#include "types.h"
#include "buf.h"

StringIntMap* keywords = NULL;

void init_types() {
    bufpush(keywords, (StringIntMap){ "imm", 0 });
    bufpush(keywords, (StringIntMap){ "mut", 0 });

    bufloop(keywords, i) {
        keywords[i].v = strlen(keywords[i].k);
    }
}

Token* token_new(
    TokenKind kind,
    const char* start,
    const char* current,
    Srcfile* srcfile,
    usize line,
    usize col,
    usize ch_count)
{
    Token* t = malloc(sizeof(Token));
    t->kind = kind;
    t->start = start;
    t->end = current;
    t->srcfile = srcfile;
    t->line = line;
    t->col = col;
    t->ch_count = ch_count;
    return t;
}

Stmt* stmt_function_def_new(FunctionHeader header, Expr* body) {
    Stmt* stmt = (Stmt*)malloc(sizeof(Stmt));
    stmt->head = header.head;
    stmt->tail = body->tail;
    stmt->kind = STMT_KIND_FUNCTION_DEF;
    stmt->funcd.header = header;
    stmt->funcd.body = body;
    return stmt;
}

Stmt* stmt_param_new(Token* identifier, Expr* type) {
    Stmt* stmt = (Stmt*)malloc(sizeof(Stmt));
    stmt->head = identifier;
    stmt->tail = type->tail;
    stmt->kind = STMT_KIND_PARAM;
    stmt->param.identifier = identifier;
    stmt->param.type = type;
    return stmt;
}
