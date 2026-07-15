#include "token.h"
#include "compile.h"

const char* tokenkind_strs[] = {
    FOREACH_TOKENKIND(STRING_GEN)
};

Token token_new(TokenKind kind, Span span) {
    Token token = (Token){
        .kind = kind,
        .span = span,
        .extra = -1
    };
    return token;
}

bool token_lexeme_eqlto(
    TokenIndex token,
    Srcfile* src, 
    const char* string
) {
    Span span = tk(src, token)->span;
    return slice_eql_to_str(
        &src->handle.contents[span.start],
        span.end - span.start,
        string
    );
}

// bool token_lexeme_eql(
//     Token* a, 
//     Token* b, 
//     Srcfile* src
// ) {
//     if ((a->span.end-a->span.start) != (b->span.end-b->span.start)) 
//         return false;
//     for (int ia = a->span.start, ib = b->span.start;
//          ia < a->span.end && ib < b->span.end;
//          ia++, ib++
//     ) {
//         if (src->handle.contents[ia] != 
//             src->handle.contents[ib]) {
//             return false;
//         }
//     }
//     return true;
// }

// char* token_tostring(Token* token, Srcfile* src) {
//     return span_tostring(token->span, src);
// }

// TODO: maybe remove this?
// char* tokenkind_tostring(TokenKind kind) {
//     return "";
// }
