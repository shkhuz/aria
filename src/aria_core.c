#define _GNU_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>

#ifdef __linux__
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

///// TYPES /////
typedef unsigned int uint;
typedef unsigned char uchar;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#define null (void*)0

///// MATH /////
/* --- NOTE: do not use expressions for arguments --- */
#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define CLAMP_MAX(x, max) MIN(x, max)
#define CLAMP_MIN(x, min) MAX(x, min)

///// ANSI COLORS /////
#define ANSI_FBOLD    "\x1B[1m"
#define ANSI_FRED     "\x1B[1;31m"
#define ANSI_FGREEN   "\x1B[1;32m"
#define ANSI_FYELLOW  "\x1B[33m"
#define ANSI_FBLUE    "\x1B[34m"
#define ANSI_FMAGENTA "\x1B[35m"
#define ANSI_FCYAN    "\x1B[1;36m"
#define ANSI_RESET    "\x1B[0m"

///// STRETCHY BUFFER /////
typedef struct {
    u64 len;
    u64 cap;
    char buf[];
} BufHdr;

#define _buf_hdr(b) ((BufHdr*)((char*)(b) - offsetof(BufHdr, buf)))

#define buf_len(b) ((b) ? _buf_hdr(b)->len : 0)
#define buf_cap(b) ((b) ? _buf_hdr(b)->cap : 0)
#define buf_end(b) ((b) + buf_len(b))
#define buf_sizeof(b) ((b) ? buf_len(b) * sizeof(*b) : 0)

#define buf_free(b)        ((b) ? (free(_buf_hdr(b)), (b) = null) : 0)
#define buf_fit(b, n)      ((n) <= buf_cap(b) ? 0 : \
                            ((b) = _buf_grow((b), (n), sizeof(*(b)))))
#define buf_push(b, ...)   (buf_fit((b), 1 + buf_len(b)), \
                            (b)[_buf_hdr(b)->len++] = (__VA_ARGS__))
#define buf_pop(b)         ((b) ? _buf_shrink((b), 1) : 0)
#define buf_clear(b)       ((b) ? _buf_hdr(b)->len = 0 : 0)
#define buf_empty(b)       ((b) ? (buf_len(b) == 0 ? true : false) : 0)
#define buf_last(b)        ((b) ? ((!buf_empty(b)) ? (b)[buf_len(b)-1] : 0) : 0)

#define buf_remove(b, n)   ((b) ? _buf_remove(b, n, sizeof(*(b))) : 0)

#define buf_loop(b, c) \
    for (u64 c = 0; c < buf_len(b); ++c)
#define buf_write(b, s) \
    for (size_t i = 0; i < strlen(s); i++) { buf_push(b, s[i]); }

void* _buf_grow(const void* buf, u64 new_len, u64 elem_size) {
    /* assert(buf_cap(buf) <= (__SIZE_MAX__ - 1) / 2); */
    u64 new_cap = CLAMP_MIN(2 * buf_cap(buf), MAX(new_len, 16));
    assert(new_len <= new_cap);
    /* assert(new_cap <= (__SIZE_MAX__ - offsetof(BufHdr, buf)) / elem_size); */
    u64 new_size = offsetof(BufHdr, buf) + (new_cap * elem_size);
    BufHdr* new_hdr;

    if (buf) {
        new_hdr = (BufHdr*)realloc(_buf_hdr(buf), new_size);
    }
    else {
        new_hdr = (BufHdr*)malloc(new_size);
        new_hdr->len = 0;
    }

    new_hdr->cap = new_cap;
    return new_hdr->buf;
}

void _buf_shrink(const void* buf, u64 size) {
    if (size > buf_len(buf)) {
        size = buf_len(buf);
    }

    _buf_hdr(buf)->len -= size;
}

void _buf_remove(const void* buf, u64 idx, u64 elem_size) {
    u64 len = buf_len(buf);
    assert(idx < len);
    assert(buf);

    u64 byte_pos = elem_size * idx;
    u64 elem_to_move_count = len - idx - 1;
    memmove(
            (u8*)buf + byte_pos, 
            (u8*)buf + byte_pos + elem_size, 
            elem_to_move_count * elem_size);
    
    BufHdr* hdr = _buf_hdr(buf);
    hdr->len = len - 1;
}

///// STRING INTERNING /////
typedef struct {
    char* str;
    u64 len;
} StrIntern;

static StrIntern* g_interns;

char* strni(char* start, char* end) {
    u64 len = end - start;
    buf_loop(g_interns, i) {
        if (g_interns[i].len == len &&
            strncmp(g_interns[i].str, start, len) == false) {
            return g_interns[i].str;
        }
    }

    char* str = (char*)malloc(len + 1);
    memcpy(str, start, len);
    str[len] = 0;
    buf_push(g_interns, (StrIntern){ str, len });
    return str;
}

char* stri(char* str) {
    return strni(str, str + strlen(str));
}

///// FILE I/O /////
typedef struct {
    char* fpath;
    char* abs_fpath;
    char* contents;
    u64 len;
} File;

typedef struct {
    File* file;
    enum {
        FILE_ERROR_SUCCESS,
        FILE_ERROR_ERROR,
        FILE_ERROR_DIR,
    } status;
} FileOrError;

static int is_dir(const char* fpath) {
    struct stat fpath_stat;
    stat(fpath, &fpath_stat);
    return S_ISDIR(fpath_stat.st_mode);
}

FileOrError file_read(const char* fpath) {
    /* TODO: more thorough error checking */
    FILE* fp = fopen(fpath, "r");
    if (!fp) {
        return (FileOrError){ null, FILE_ERROR_ERROR };
    }

    if (is_dir(fpath)) {
        return (FileOrError){ null, FILE_ERROR_DIR };
    }

    fseek(fp, 0, SEEK_END);
    u64 size = ftell(fp);
    rewind(fp);

    char* contents = (char*)malloc(size + 1);
    fread((void*)contents, sizeof(char), size, fp);
    contents[size] = '\0';
    fclose(fp);

    char abs_fpath[PATH_MAX+1];
    char* abs_fpath_ptr = realpath(fpath, abs_fpath);
    if (!abs_fpath_ptr) {
        return (FileOrError){ null, FILE_ERROR_ERROR };
    }

    File* file = malloc(sizeof(File));
    file->fpath = stri((char*)fpath);
    file->abs_fpath = stri(abs_fpath_ptr);
    file->contents = contents;
    file->len = size;
    return (FileOrError){ file, FILE_ERROR_SUCCESS };
}

bool file_exists(const char* fpath) {
    FILE* fp = fopen(fpath, "r");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

///// STRING OPS /////
char* aria_strsub(char* str, uint start, uint len) {
    if (!str) {
        return null;
    }

    if (len == 0) {
        return null;
    }

    char* res = null;
    for (uint i = start; i < start + len; i++) {
        buf_push(res, str[i]);
    }
    buf_push(res, '\0');
    return res;
}

char* aria_strapp(char* to, char* from) {
    if (!to || !from) {
        return null;
    }

    char* res = null;
    uint to_len = strlen(to);
    uint from_len = strlen(from);
    for (uint i = 0; i < to_len; i++) {
        buf_push(res, to[i]);
    }
    for (uint i = 0; i < from_len; i++) {
        buf_push(res, from[i]);
    }
    buf_push(res, '\0');
    return res;
}

char* aria_basename(char* fpath) {
    if (!fpath) {
        return null;
    } 

    char* last_dot_ptr = strrchr(fpath, '.');
    if (last_dot_ptr == null) {
        return fpath;
    }

    return aria_strsub(fpath, 0, last_dot_ptr - fpath);
}

char* aria_notdir(char* fpath) {
    if (!fpath) {
        return null;
    } 

    uint fpath_len = strlen(fpath);
    char* last_slash_ptr = strrchr(fpath, '/');
    if (last_slash_ptr == null) {
        return fpath;
    }

    return aria_strsub(fpath, last_slash_ptr - fpath + 1, fpath_len);
}   

///// MEMORY /////
#define alloc_with_type(name, type) type* name = malloc(sizeof(type));
#define stack_arr_len(arr) (sizeof(arr) / sizeof(*arr))

///// MISC /////
#define COMBINE1(X, Y) X##Y
#define COMBINE(X,Y) COMBINE1(X,Y)
