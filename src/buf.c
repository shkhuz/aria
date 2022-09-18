#include "buf.h"

void* _bufgrow(const void* buf, usize new_len, usize elem_size) {
    usize new_cap = CLAMP_MIN(2 * bufcap(buf), MAX(new_len, 16));
    assert(new_len <= new_cap);

    usize mem_to_alloc = new_cap * elem_size + offsetof(bufhdr, data);
    bufhdr* new_hdr;
    if (buf) {
        new_hdr = (bufhdr*)realloc(_bufhdr(buf), mem_to_alloc);
    }
    else {
        new_hdr = (bufhdr*)malloc(mem_to_alloc);
        new_hdr->len = 0;
    }

    new_hdr->cap = new_cap;
    return new_hdr->data;
}
