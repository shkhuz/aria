#include "misc.h"
#include "buf.h"

StringKwKind* keywords = NULL;

void init_types() {
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

char* aria_format(const char* fmt, ...) {
    char* buf;
    va_list args;
    va_start(args, fmt);
    vasprintf(&buf, fmt, args);
    va_end(args);
    return buf;
}
