#ifndef TMP_H
#define TMP_H

#include "core.h"

typedef struct {
    char* base;
    u64 capacity;
    u64 pos;
} Arena;
Arena arena_create(u64 reserve);
void arena_destroy(Arena* arena);
void arena_clear(Arena* arena);
void* arena_push(Arena* arena, u64 size);

#define SB_CHUNK_SHIFT 20
#define SB_CHUNK_SIZE  (1ULL << SB_CHUNK_SHIFT)
#define SB_CHUNK_MASK  (SB_CHUNK_SIZE - 1)

typedef struct {
    Arena* arena;
    usize cap;
    usize len;
    usize elem_size;
    u32 slow_capacity;
    u32 slow_count;
    void** slow_path;
    char data[];
} pbufhdr;

void* _pbufgrow(Arena *arena, const void* buf, usize new_len, usize elem_size);
void* _pbuf_get_slow(pbufhdr *hdr, usize index);
void pbuf_debug_dump_slow_chunks(const void *buf);

#define _pbufhdr(b)      ((pbufhdr*)((char*)(b) - offsetof(pbufhdr, data)))
#define pbuflen(b)       ((b) ? _pbufhdr((b))->len : 0)
#define pbufcap(b)       ((b) ? _pbufhdr((b))->cap : 0)
#define pbufend(b)       ((b) + pbuflen(b))
#define pbuflastidx(b)   (pbuflen(b) - 1)
#define pbuflast(b)      (pbuflen((b)) == 0 ? (NULL) : &pbufget((b), pbuflastidx(b)))

#define pbufget(b, i) (*((i) < SB_CHUNK_SIZE ? \
    &(b)[(i)] : \
    ((__typeof__(*(b)) *)_pbuf_get_slow(_pbufhdr(b), (i))) \
))

#define pbuffit(arena, b, n) (((b) && pbufcap(b) >= (n)) ? 0 : \
    ((b) = _pbufgrow((arena), (b), (n), sizeof(*(b)))))

#define pbufpush(arena, b, ...) (pbuffit((arena), (b), 1 + pbuflen((b))), \
    (pbufget((b), _pbufhdr((b))->len) = __VA_ARGS__), \
    _pbufhdr((b))->len++)

#define pbufpop(b) (pbuflen(b) > 0 ? pbufget((b), --_pbufhdr((b))->len) : 0)

#define pbufclear(b) ((b) ? _pbufhdr((b))->len = 0 : 0)
#define pbuffree(b)  ((b) ? (b=NULL) : 0) 

#define pbufloop(b, c) for (usize c = 0; c < pbuflen(b); c++)
#define pbufrevloop(b, c) for (usize c = pbuflen(b); c-- > 0 ;)

#endif
