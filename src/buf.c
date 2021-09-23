#include "buf.h"

void* _buf_grow(const void* buf, size_t new_len, size_t elem_size) {
    /* assert(buf_cap(buf) <= (__SIZE_MAX__ - 1) / 2); */
    size_t new_cap = CLAMP_MIN(2 * buf_cap(buf), MAX(new_len, 16));
    assert(new_len <= new_cap);
    /* assert(new_cap <= (__SIZE_MAX__ - offsetof(BufHdr, buf)) / elem_size); */
    size_t new_size = offsetof(BufHdr, buf) + (new_cap * elem_size);

    BufHdr* new_hdr;
    if (buf) {
        new_hdr = (BufHdr*)aria_realloc(_buf_hdr(buf), new_size);
    } 
    else {
        new_hdr = (BufHdr*)aria_malloc(new_size);
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

char* _buf_vprintf(char* buf, const char* fmt, va_list ap) {
    va_list aq;
    va_copy(aq, ap);
    size_t cap = buf_cap(buf) - buf_len(buf);
    size_t n = 1 + vsnprintf(buf_end(buf), cap, fmt, aq);
    va_end(aq);

    if (n > cap) {
        buf_fit(buf, n + buf_len(buf));
        va_copy(aq, ap);
        size_t new_cap = buf_cap(buf) - buf_len(buf);
        n = 1 + vsnprintf(buf_end(buf), new_cap, fmt, aq);
        assert(n <= new_cap);
        va_end(aq);
    }

    _buf_hdr(buf)->len += n - 1;
    return buf;
}

char* _buf_printf(char* buf, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char* res = _buf_vprintf(buf, fmt, ap);
    va_end(ap);
    return res;
}
