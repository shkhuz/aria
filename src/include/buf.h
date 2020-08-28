#ifndef _BUF_H
#define _BUF_H

#include <types.h>

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

#define buf_free(b)		   ((b) ? (free(_buf_hdr(b)), (b) = null) : 0)
#define buf_fit(b, n)	   ((n) <= buf_cap(b) ? 0 :	\
							((b) = _buf_grow((b), (n), sizeof(*(b)))))
#define buf_push(b, ...)   (buf_fit((b), 1 + buf_len(b)), \
							(b)[_buf_hdr(b)->len++] = (__VA_ARGS__))
#define buf_pop(b)		   ((b) ? _buf_shrink((b), 1) : (void)0)
#define buf_remove(b, n)   ((b) ? _buf_remove((b), (n), sizeof(*(b))) : (void)0)
#define buf_clear(b)	   ((b) ? _buf_hdr(b)->len = 0 : 0)
#define buf_empty(b)	   ((b) ? (buf_len(b) == 0 ? true : false) : 0)
#define buf_last(b)		   ((b) ? ((!buf_empty(b)) ? (b)[buf_len(b)-1] : 0) : 0)

#define buf_loop(b, c) for (u64 c = 0; c < buf_len(b); ++c)

void* _buf_grow(const void* buf, u64 new_len, u64 elem_size);
void _buf_shrink(const void* buf, u64 size);
void _buf_remove(const void* buf, u64 idx, u64 elem_size);

#endif /* _BUF_H */
