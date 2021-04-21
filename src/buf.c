#include <aria_core.h>

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
	memmove((u8*)buf + byte_pos, (u8*)buf + byte_pos + elem_size, elem_to_move_count * elem_size);
	
	BufHdr* hdr = _buf_hdr(buf);
	hdr->len = len - 1;
}
