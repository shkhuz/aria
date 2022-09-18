#include "printf.h"
#include "core.h"
#include "buf.h"

static void aprintc(char** out, char c) {
    if (out)
        bufpush(*out, c);
    else
        putchar(c);
}

static int aprints(char** out, const char* s) {
    register int pc = 0;

    for (; *s; ++s) {
        aprintc(out, *s);
        ++pc;
    }

    return pc;
}

typedef enum {
    FSK_SIGNED_INT,
    FSK_UNSIGNED_INT,
    FSK_LOWER_HEX,
    FSK_UPPER_HEX,
    FSK_OCTAL
} FmtSpecifierKind;

static int aprint(char** out, const char* fmt, va_list args) {
    register int pc = 0;

    for (; *fmt != 0;) {
        if (*fmt == '%') {
            ++fmt;

            char size_specifier[2] = { ' ', ' ' };
            if (*fmt == 'h' ||
                *fmt == 'l' ||
                *fmt == 'z') {
                size_specifier[0] = *fmt;
                ++fmt;
            }

            if (*fmt == '%') {
                aprintc(out, *fmt);
                ++pc;
            }
            else if (*fmt == 's') {
                char* s = va_arg(args, char*);
                pc += aprints(out, s?s:"(null)");
                ++fmt;
            }
            else if (*fmt == 'c') {
                char c = va_arg(args, int);
                aprintc(out, c);
                ++pc;
                ++fmt;
            }
            else if (*fmt == 'd' || *fmt == 'i' || *fmt == 'u') {
                FmtSpecifierKind speckind;
                switch (*fmt) {
                    case 'd':
                    case 'i': {
                        speckind = FSK_SIGNED_INT;
                    } break;

                    case 'u': {
                        speckind = FSK_UNSIGNED_INT;
                    } break;

                    default: {
                        assert(0);
                        break;
                    }
                }
                ++fmt;
                
                usize unsg;
                ssize sg;
                if (speckind == FSK_SIGNED_INT) {
                    if (strncmp(size_specifier, "h", 1) == 0) sg = va_arg(args, /*short*/ int);
                    else if (strncmp(size_specifier, "l", 1) == 0) sg = va_arg(args, long int);
                    else if (strncmp(size_specifier, "z", 1) == 0) sg = va_arg(args, size_t);
                    else if (strncmp(size_specifier, "  ", 2) == 0) sg = va_arg(args, int);
                    else assert(0);
                }
                else if (speckind == FSK_UNSIGNED_INT) {
                    if (strncmp(size_specifier, "h", 1) == 0) unsg = va_arg(args, unsigned /*short*/ int);
                    else if (strncmp(size_specifier, "l", 1) == 0) unsg = va_arg(args, unsigned long int);
                    else if (strncmp(size_specifier, "z", 1) == 0) unsg = va_arg(args, size_t);
                    else if (strncmp(size_specifier, "  ", 2) == 0) unsg = va_arg(args, unsigned int);
                    else assert(0);
                }
                
                bool neg = false;
                usize i;
                if (speckind == FSK_SIGNED_INT) {
                    if (sg < 0) {
                        i = (usize)(-sg);
                        neg = true;
                    }
                    else i = (usize)sg;
                }
                else i = unsg;

                char* buf = NULL;
                if (neg == true) bufpush(buf, '-');
                while (i != 0) {
                    char c = (char)(i % 10);
                    bufpush(buf, c + '0');
                    i /= 10;
                }
                
                usize len = buflen(buf);
                usize digitslen = neg ? len-1 : len;
                for (usize idx = neg ? 1 : 0; idx < digitslen/2; idx++) {
                    char tmp = *(buflast(buf)-idx);
                    *(buflast(buf)-idx) = buf[idx];
                    buf[idx] = tmp;
                }
                if (out) {
                    buffree(*out);
                    *out = buf;
                }
                else {
                    for (usize idx = 0; idx < len; idx++) {
                        aprintc(out, buf[idx]);
                    }
                    buffree(buf);
                }
            }
        }
        else {
            aprintc(out, *fmt);
            ++pc;
            ++fmt;
        }
    }

    return pc;
}

int aprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    register int pc = aprint(NULL, fmt, args);
    va_end(args);
    return pc;
}
