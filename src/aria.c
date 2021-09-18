#include "aria.h"

void _aria_fprintf(
        const char* calleefile, 
        size_t calleeline, 
        FILE* file, 
        const char* fmt, 
        ...) {
    va_list ap;
    va_start(ap, fmt);

    size_t fmtlen = strlen(fmt);
    for (size_t i = 0; i < fmtlen; i++) {
        if (fmt[i] == '{') {
            bool found_rbrace = false;
            i++;

            switch (fmt[i]) {
                case 'q': {
                    i++;
                    switch (fmt[i]) {
                        case 'i': {
                            fprintf(file, "%li", va_arg(ap, int64_t));
                        } break;

                        case 'u': {
                            fprintf(file, "%lu", va_arg(ap, uint64_t));
                        } break;

                        default: {
                            fprintf(
                                    stderr, 
                                    "aria_fprintf: (%s:%lu) expect [u|i] "
                                    "after `q`\n", 
                                    calleefile, 
                                    calleeline);
                            return;
                        } break;
                    }
                    i++;
                } break;

                case 't': {
                    i++;
                    if (fmt[i] == 'k') {
                        i++;
                        fprintf_token(file, va_arg(ap, Token*));
                    }
                    else {
                        fprint_type(file, va_arg(ap, Type*));
                    }
                } break;

                case 'c': {
                    i++;
                    fprintf(file, "%c", va_arg(ap, int));
                } break;

                case 's': {
                    i++;
                    fprintf(file, "%s", va_arg(ap, char*));
                } break;

                case '}': {
                    fprintf(file, "%d", va_arg(ap, int));
                    found_rbrace = true; 
                } break;

                default: {
                    fprintf(
                            stderr, 
                            "aria_fprintf: (%s:%lu) unknown specifier `%c`\n", 
                            calleefile, 
                            calleeline, 
                            fmt[i]);
                    return;
                } break;
            }
            
            if (!found_rbrace) {
                if (fmt[i] != '}') {
                    fprintf(
                            stderr, 
                            "aria_fprintf: (%s:%lu) unexpected eos while "
                            "finding `}`\n", 
                            calleefile, 
                            calleeline);
                    return;
                }
            }
        } 
        else {
            putc(fmt[i], file);
        }
    }

    va_end(ap);
}

void fprint_type(FILE* file, Type* type) {
    if (type->ptr) {
        putc('*', file);
    }
    fprintf(file, "%s", type->name);
}

void fprintf_token(FILE* file, Token* token) {
    fprintf(file, "%s", token->lexeme);
}
