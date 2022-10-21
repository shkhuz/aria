#include "printf.h"
#include "buf.h"
#include "token.h"

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

static int avprint(void* out, bool out_is_file, const char* fmt, va_list args);

// out_is_file (when out != NULL):
//                true  -> file
//                false -> buffer
static void aprintc(void* out, bool out_is_file, char c) {
    if (out && out_is_file) fputc(c, (FILE*)out);
    else if (out && !out_is_file) {
        bufpush(*((char**)out), c);
    }
    else putchar(c);
}

static int aprints(void* out, bool out_is_file, const char* s, int len) {
    register int pc = 0;
    if (len) {
        for (int i = 0; i < len; ++i) {
            aprintc(out, out_is_file, s[i]);
            ++pc;
        }
    } else {
        for (; *s; ++s) {
            aprintc(out, out_is_file, *s);
            ++pc;
        }
    }
    return pc;
}

static int aprint(void* out, bool out_is_file, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int pc = avprint(out, out_is_file, fmt, args);
    va_end(args);
    return pc;
}

static int pad(void* out, bool out_is_file, int srclen, int* width, int padding) {
    register int pc = 0;
    char padchar = ' ';
    if (*width > 0) {
        if (srclen >= *width) *width = 0;
        else *width -= srclen;
        
        if (padding & FPAD_ZERO) padchar = '0';
    }
    
    if (!(padding & FPAD_RIGHT)) {
        for (; *width > 0; --(*width)) {
            aprintc(out, out_is_file, padchar);
            ++pc;
        }
    }
    return pc;
}

static int avprint(void* out, bool out_is_file, const char* fmt, va_list args) {
    register int pc = 0;
    int width, padding;

    for (; *fmt != 0;) {
        if (*fmt == '%') {
            ++fmt;
            width = padding = 0;
            
            if (*fmt == '%') {
                aprintc(out, out_is_file, *fmt);
                ++fmt;
                ++pc;
            }
            else {
                if (*fmt == '-') {
                    ++fmt;
                    padding = FPAD_RIGHT;
                }
                while (*fmt == '0') {
                    ++fmt;
                    padding |= FPAD_ZERO;
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

                if (*fmt == 's') {
                    char* s = va_arg(args, char*);
                    pc += pad(out, out_is_file, strlen(s), &width, padding);
                    pc += aprints(out, out_is_file, s?s:"(null)", 0);
                    ++fmt;
                }
                else if (*fmt == 'c') {
                    static char buf[2];
                    buf[0] = va_arg(args, int);
                    buf[1] = '\0';
                    pc += pad(out, out_is_file, 1, &width, padding);
                    aprints(out, out_is_file, buf, 0);
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

                    int numlen = 0;
                    usize i2 = i;
                    while (i2 != 0) {
                        numlen++;
                        i2 /= 10;
                    }
                    if (neg) numlen++;

                    char* buf = NULL;
                    int numpadded = pad(&buf, false, numlen, &width, padding); 
                    pc += numpadded;
                    pc += numlen;

                    if (neg == true) bufpush(buf, '-');
                    if (i == 0) bufpush(buf, '0');
                    while (i != 0) {
                        char c = (char)(i % 10);
                        bufpush(buf, c + '0');
                        i /= 10;
                    }
                    
                    for (int idx = neg ? 1 : 0; idx < numlen/2; idx++) {
                        char tmp = *(buflast(buf)-idx);
                        *(buflast(buf)-idx) = buf[idx];
                        buf[idx] = tmp;
                    }

                    if (out && !out_is_file) {
                        buffree(*((char**)out));
                        *((char**)out) = buf;
                    }
                    else {
                        for (int idx = 0; idx < numlen + numpadded; idx++) {
                            aprintc(out, out_is_file, buf[idx]);
                        }
                        buffree(buf);
                    }
                }
                else if (*fmt == 't' && fmt[1] == 'o') {
                    fmt += 2;
                    Token* t = va_arg(args, Token*);
                    aprints(out, out_is_file, t->start, t->ch_count);
                }

                if (padding & FPAD_RIGHT) {
                    char padchar = ' ';
                    if (width > 0 && (padding & FPAD_ZERO)) padchar = '0';
                    for (; width > 0; --width) {
                        aprintc(out, out_is_file, padchar);
                        ++pc;
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
    register int pc = avprint(NULL, 0, fmt, args);
    va_end(args);
    return pc;
}

int avprintf(const char* fmt, va_list args) {
    return avprint(NULL, 0, fmt, args);
}

int afprintf(FILE* file, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    register int pc = avprint((void*)file, true, fmt, args);
    va_end(args);
    return pc;
}

int avfprintf(FILE* file, const char* fmt, va_list args) {
    return avprint((void*)file, true, fmt, args);
}

int asprintf(char** buf, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    register int pc = avprint((void*)buf, false, fmt, args);
    va_end(args);
    return pc;
}

int avsprintf(char** buf, const char* fmt, va_list args) {
    return avprint((void*)buf, false, fmt, args);
}
