#include "printf.h"
#include "buf.h"

typedef enum {
    FPAD_RIGHT = 1,
    FPAD_ZERO = 2,
} FmtPadding;

typedef enum {
    FSK_SIGNED_INT,
    FSK_UNSIGNED_INT,
    FSK_LOWER_HEX,
    FSK_UPPER_HEX,
    FSK_OCTAL
} FmtSpecifierKind;

// out_is_file (when out != NULL):
//                true  -> file
//                false -> buffer
static void aprintc(void* out, bool out_is_file, char c) {
    if (out && out_is_file) fputc(c, (FILE*)out);
    else if (out && !out_is_file) bufpush(*((char**)out), c);
    else putchar(c);
}

static int aprints(void* out, bool out_is_file, int width, int pad, const char* s) {
    register int pc = 0;
    char padchar = ' ';

    if (width > 0) {
        register int len = strlen(s);
        if (len >= width) width = 0;
        else width -= len;
        
        if (pad & FPAD_ZERO) padchar = '0';
    }
    
    if (!(pad & FPAD_RIGHT)) {
        for (; width > 0; --width) {
            aprintc(out, out_is_file, padchar);
            ++pc;
        }
    }

    for (; *s; ++s) {
        aprintc(out, out_is_file, *s);
        ++pc;
    }

    for (; width > 0; --width) {
        aprintc(out, out_is_file, padchar);
        ++pc;
    }

    return pc;
}

static int aprint(void* out, bool out_is_file, const char* fmt, va_list args) {
    register int pc = 0;
    int width, pad;

    for (; *fmt != 0;) {
        if (*fmt == '%') {
            ++fmt;
            width = pad = 0;
            
            if (*fmt == '%') {
                aprintc(out, out_is_file, *fmt);
                ++fmt;
                ++pc;
            }
            else {
                if (*fmt == '-') {
                    ++fmt;
                    pad = FPAD_RIGHT;
                }
                while (*fmt == '0') {
                    ++fmt;
                    pad |= FPAD_ZERO;
                }
                for (; *fmt > '0' && *fmt <= '9'; ++fmt) {
                    width *= 10;
                    width += *fmt - '0';
                }
                
                char size_specifier[2] = { ' ', ' ' };
                if (*fmt == 'h' ||
                    *fmt == 'l' ||
                    *fmt == 'z') {
                    size_specifier[0] = *fmt;
                    ++fmt;
                }

                else if (*fmt == 's') {
                    char* s = va_arg(args, char*);
                    pc += aprints(out, out_is_file, width, pad, s?s:"(null)");
                    ++fmt;
                }
                else if (*fmt == 'c') {
                    static char buf[2];
                    buf[0] = va_arg(args, int);
                    buf[1] = '\0';
                    aprints(out, out_is_file, width, pad, buf);
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
                    if (out && !out_is_file) {
                        buffree(*((char**)out));
                        *((char**)out) = buf;
                    }
                    else {
                        for (usize idx = 0; idx < len; idx++) {
                            aprintc(out, out_is_file, buf[idx]);
                        }
                        buffree(buf);
                    }
                }
            }
        }
        else {
            aprintc(out, out_is_file, *fmt);
            ++pc;
            ++fmt;
        }
    }

    return pc;
}

int aprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    register int pc = aprint(NULL, 0, fmt, args);
    va_end(args);
    return pc;
}

int avprintf(const char* fmt, va_list args) {
    return aprint(NULL, 0, fmt, args);
}

int afprintf(FILE* file, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    register int pc = aprint((void*)file, true, fmt, args);
    va_end(args);
    return pc;
}

int avfprintf(FILE* file, const char* fmt, va_list args) {
    return aprint((void*)file, true, fmt, args);
}

int asprintf(char** buf, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    register int pc = aprint((void*)buf, false, fmt, args);
    va_end(args);
    return pc;
}

int avsprintf(char** buf, const char* fmt, va_list args) {
    return aprint((void*)buf, false, fmt, args);
}
