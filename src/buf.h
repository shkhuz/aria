#ifndef BUF_H
#define BUF_H

#include "core.h"

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

#define buf_free(b)        ((b) ? (aria_free(_buf_hdr(b)), (b) = null) : 0)
#define buf_fit(b, n)      ((n) <= buf_cap(b) ? 0 : \
                            ((b) = _buf_grow((b), (n), sizeof(*(b)))))
#define buf_push(b, ...)   (buf_fit((b), 1 + buf_len(b)), \
                            (b)[_buf_hdr(b)->len++] = (__VA_ARGS__))
#define buf_pop(b)         ((b) ? _buf_shrink((b), 1) : 0)
#define buf_clear(b)       ((b) ? _buf_hdr(b)->len = 0 : 0)
#define buf_empty(b)       ((b) ? (buf_len(b) == 0 ? true : false) : 0)
#define buf_last(b)        ((b) ? ((!buf_empty(b)) ? (b)[buf_len(b)-1] : 0) : 0)

#define buf_remove(b, n)   ((b) ? _buf_remove(b, n, sizeof(*(b))) : 0)

#define buf_vprintf(b, ...) ((b) = _buf_vprintf((b), __VA_ARGS__))
#define buf_printf(b, ...) ((b) = _buf_printf((b), __VA_ARGS__))
#define buf_loop(b, c) \
    for (size_t c = 0; c < buf_len(b); ++c)
#define buf_write(b, s) \
    for (size_t i = 0; i < strlen(s); i++) { buf_push(b, s[i]); }

void* _buf_grow(const void* buf, size_t new_len, size_t elem_size);
void _buf_shrink(const void* buf, size_t size);
void _buf_remove(const void* buf, size_t idx, size_t elem_size);
char* _buf_vprintf(char* buf, const char* fmt, va_list ap);
char* _buf_printf(char* buf, const char* fmt, ...);

#endif
