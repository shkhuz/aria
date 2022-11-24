#ifndef BUF_H
#define BUF_H

#include "core.h"

typedef struct {
    usize cap;
    usize len;
    char data[];
} bufhdr;

#define _bufhdr(b) ((bufhdr*)((char*)(b) - offsetof(bufhdr, data)))
#define buflen(b) ((b) ? _bufhdr(b)->len : 0)
#define bufcap(b) ((b) ? _bufhdr(b)->cap : 0)
#define bufend(b) ((b) + buflen(b))
#define buflast(b) (buflen(b) == 0 ? (b) : (bufend(b)-1))

#define buffit(b, n) (bufcap(b) >= n ? 0 : \
                        ((b) = _bufgrow((b), (n), sizeof(*(b)))))
#define bufpush(b, ...) (buffit((b), 1 + buflen((b))), \
                         (b)[_bufhdr((b))->len++] = __VA_ARGS__)
#define buffree(b) ((b) ? (free(_bufhdr(b)), b=NULL) : 0)

#define bufloop(b, c) for (usize c = 0; c < buflen(b); c++) 

void* _bufgrow(const void* buf, usize new_len, usize elem_size);

#endif