#include "token.h"

const char* tokenkind_strs[] = {
    FOREACH_TOKENKIND(STRING_GEN)
};

const StringTokenKindTup keywords[] = {
    { "comp",       TK_KW_COMP },
    { "fun",        TK_KW_FUN },
    { "imm",        TK_KW_IMM },
    { "mut",        TK_KW_MUT },
    { "struct",     TK_KW_STRUCT },
    { "yield",      TK_KW_YIELD },
};
const int KEYWORDS_LEN = (int)ARRAY_LEN(keywords);

StrlitData* token_strlit_data = NULL;

Token* token_new(TokenKind kind, Span span) {
    Token* token = ALLOC_OBJ(Token);
    token->kind = kind;
    token->span = span;
    return token;
}

bool token_lexeme_eqlto(Token* token, const char* string) {
    return slice_eql_to_str(
        &token->span.srcfile->handle.contents[token->span.start],
        token->span.end - token->span.start,
        string
    );
}

bool token_lexeme_eql(Token* a, Token* b) {
    if ((a->span.end-a->span.start) != (b->span.end-b->span.start)) 
        return false;
    for (usize ia = a->span.start, ib = b->span.start;
         ia < a->span.end && ib < b->span.end;
         ia++, ib++
    ) {
        if (a->span.srcfile->handle.contents[ia] != 
            b->span.srcfile->handle.contents[ib]) {
            return false;
        }
    }
    return true;
}

char* token_tostring(Token* token) {
    return span_tostring(token->span);
}

char* tokenkind_tostring(TokenKind kind) {
    return "";
}
