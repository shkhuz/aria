#ifndef BUF_H
#define BUF_H

#include "core.h"

typedef struct {
    usize cap;
    usize len;
    char data[];
} bufhdr;

#define _bufhdr(b) ((bufhdr*)((char*)(b) - offsetof(bufhdr, data)))
#define bufend(b) ((b) + buflen(b))
#define buflast(b) (buflen((b)) == 0 ? (NULL) : (bufend((b))-1))

#define buffit(b, n) (bufcap(b) >= n ? 0 : \
                        ((b) = _bufgrow((b), (n), sizeof(*(b)))))

#define bufpush(b, ...) (buffit((b), 1 + buflen((b))), \
                         ((b)[_bufhdr((b))->len++] = __VA_ARGS__))

#define bufstrexpandpush(b, e) {usize COMBINE(__tmpsize, __LINE__) = strlen((e)); \
                                (buffit((b), (COMBINE(__tmpsize, __LINE__)) + buflen((b))), \
                                (memcpy(&((b)[_bufhdr((b))->len]), (e), (COMBINE(__tmpsize, __LINE__)))), \
                                (_bufhdr((b))->len += (COMBINE(__tmpsize, __LINE__))));}

#define bufpop(b) (buflen(b) > 0 ? (_bufhdr((b))->len--) : 0)

#define buffree(b) ((b) ? (free(_bufhdr(b)), b=NULL) : 0)

#define bufloop(b, c) for (usize c = 0; c < buflen(b); c++)
#define bufrevloop(b, c) for (usize c = buflen(b); c-- > 0 ;)

#define bufinsert(b, i, ...) (buffit((b), 1 + buflen((b))), \
                              memmove((b+i+1), (b+i), (_bufhdr((b))->len-i) * sizeof(*b)), \
                              ((b)[i] = __VA_ARGS__), \
                              _bufhdr((b))->len++)
#define bufclear(b) ((b) ? _bufhdr((b))->len = 0 : 0)

usize buflen(const void* buf);
usize bufcap(const void* buf);
void* _bufgrow(const void* buf, usize new_len, usize elem_size);

#endif
