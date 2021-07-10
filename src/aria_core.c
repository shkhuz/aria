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
typedef u64 u128 __attribute__((mode(TI)));

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef i64 i128 __attribute__((mode(TI)));

#define null (void*)0

///// MATH /////
/* --- NOTE: do not use expressions for arguments --- */
#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define CLAMP_MAX(x, max) MIN(x, max)
#define CLAMP_MIN(x, min) MAX(x, min)

int round_to_next_multiple(int n, int multiple) {
	if (multiple == 0) return n;

	int remainder = n % multiple;
	if (remainder == 0) return n;

	return n + multiple - remainder;
}

size_t get_bits_for_value(u128 n) {
	size_t count = 0;
	while (n > 0) {
		n = n >> 1;
		count++;
	}
	return count;
}

///// ERROR CODES /////

typedef enum {
	ERROR_CODE_OUT_OF_MEM,
	ERROR_CODE_OKAY,
} ErrorCode;

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
    size_t len;
    size_t cap;
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

#define buf_printf(b, ...) ((b) = buf__printf((b), __VA_ARGS__))
#define buf_loop(b, c) \
    for (size_t c = 0; c < buf_len(b); ++c)
#define buf_write(b, s) \
    for (size_t i = 0; i < strlen(s); i++) { buf_push(b, s[i]); }

void* _buf_grow(const void* buf, size_t new_len, size_t elem_size) {
    /* assert(buf_cap(buf) <= (__SIZE_MAX__ - 1) / 2); */
    size_t new_cap = CLAMP_MIN(2 * buf_cap(buf), MAX(new_len, 16));
    assert(new_len <= new_cap);
    /* assert(new_cap <= (__SIZE_MAX__ - offsetof(BufHdr, buf)) / elem_size); */
    size_t new_size = offsetof(BufHdr, buf) + (new_cap * elem_size);
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

void _buf_shrink(const void* buf, size_t size) {
    if (size > buf_len(buf)) {
        size = buf_len(buf);
    }

    _buf_hdr(buf)->len -= size;
}

void _buf_remove(const void* buf, size_t idx, size_t elem_size) {
    size_t len = buf_len(buf);
    assert(idx < len);
    assert(buf);

    size_t byte_pos = elem_size * idx;
    size_t elem_to_move_count = len - idx - 1;
    memmove(
            (u8*)buf + byte_pos, 
            (u8*)buf + byte_pos + elem_size, 
            elem_to_move_count * elem_size);
    
    BufHdr* hdr = _buf_hdr(buf);
    hdr->len = len - 1;
}

char* buf__printf(char* buf, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    size_t cap = buf_cap(buf) - buf_len(buf);
    size_t n = 1 + vsnprintf(buf_end(buf), cap, fmt, ap);
    va_end(ap);

    if (n > cap) {
        buf_fit(buf, n + buf_len(buf));
        va_start(ap, fmt);
        size_t new_cap = buf_cap(buf) - buf_len(buf);
        n = 1 + vsnprintf(buf_end(buf), new_cap, fmt, ap);
        assert(n <= new_cap);
        va_end(ap);
    }

    _buf_hdr(buf)->len += n - 1;
    return buf;
}

///// STRING INTERNING /////
typedef struct {
    char* str;
    size_t len;
} StrIntern;

static StrIntern* g_interns;

char* strni(char* start, char* end) {
    size_t len = end - start;
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
    size_t len;
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
    size_t size = ftell(fp);
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
    size_t to_len = strlen(to);
    size_t from_len = strlen(from);
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

    size_t fpath_len = strlen(fpath);
    char* last_slash_ptr = strrchr(fpath, '/');
    if (last_slash_ptr == null) {
        return fpath;
    }

    return aria_strsub(fpath, last_slash_ptr - fpath + 1, fpath_len);
}   

///// MEMORY /////
#define alloc_with_type(name, type) type* name = malloc(sizeof(type))
#define alloc_with_type_no_decl(name, type) name = malloc(sizeof(type))
#define zero_memory(mem, count) (memset(mem, 0, count * sizeof(*mem)))
#define stack_arr_len(arr) (sizeof(arr) / sizeof(*arr))
#define sizeof_in_bits(x) ((size_t)8 * sizeof(x))
#define swap_vars(t, a, b) do { t _c = a; a = b; b = _c; } while (0)

///// MISC /////
#define COMBINE1(X, Y) X##Y
#define COMBINE(X,Y) COMBINE1(X,Y)

#include <bigint.c>
