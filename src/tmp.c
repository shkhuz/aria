#include "tmp.h"

Arena arena_create(u64 reserve) {
    Arena arena = (Arena){};
    void* ptr = mmap(NULL, reserve, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (ptr != MAP_FAILED) {
        arena.base = ptr;
        arena.capacity = reserve;
    }
    return arena;
}

void arena_destroy(Arena* arena) {
    if (arena->base) {
        munmap(arena->base, arena->capacity);
    }
}

void arena_clear(Arena* arena) {
    arena->pos = 0;
}

void* arena_push(Arena* arena, u64 size) {
    u64 aligned_size = (size + 7) & ~7;
    assert(arena->pos + aligned_size <= arena->capacity && "Arena out of memory!");
    void* ptr = arena->base + arena->pos;
    arena->pos += aligned_size;
    return ptr;
}

void* _pbufgrow(Arena *arena, const void* buf, usize new_len, usize elem_size) {
    if (!buf) {
        usize fast_path_slots = SB_CHUNK_SIZE;
        while (fast_path_slots < new_len) {
            fast_path_slots += SB_CHUNK_SIZE; 
        }

        usize header_bytes = offsetof(pbufhdr, data);
        usize data_bytes = fast_path_slots * elem_size;
        
        pbufhdr *hdr = (pbufhdr*)arena_push(arena, header_bytes + data_bytes);
        
        hdr->arena = arena;
        hdr->cap = fast_path_slots;
        hdr->len = 0;
        hdr->elem_size = elem_size;
        
        return hdr->data;
    }

    pbufhdr *hdr = _pbufhdr(buf);
    assert(hdr->arena == arena);

    while (hdr->cap < new_len) {
        uint32_t current_slow_idx = (uint32_t)((hdr->cap - SB_CHUNK_SIZE) >> SB_CHUNK_SHIFT);

        if (current_slow_idx >= hdr->slow_capacity) {
            uint32_t old_capacity = hdr->slow_capacity;
            hdr->slow_capacity = hdr->slow_capacity == 0 ? 4 : hdr->slow_capacity * 2;
            
            void **new_slow_path = arena_push(arena, hdr->slow_capacity * sizeof(void*));
            if (old_capacity > 0) {
                memcpy(new_slow_path, hdr->slow_path, old_capacity * sizeof(void*));
            }
            hdr->slow_path = new_slow_path;
        }

        usize chunk_bytes = SB_CHUNK_SIZE * elem_size;
        hdr->slow_path[current_slow_idx] = arena_push(arena, chunk_bytes);
        
        hdr->slow_count++;
        hdr->cap += SB_CHUNK_SIZE;
    }

    return hdr->data;
}

void* _pbuf_get_slow(pbufhdr *hdr, usize index) {
    usize slow_index = index - SB_CHUNK_SIZE;
    uint32_t chunk_idx = (uint32_t)(slow_index >> SB_CHUNK_SHIFT);
    uint32_t local_idx = (uint32_t)(slow_index & SB_CHUNK_MASK);
    
    assert(chunk_idx < hdr->slow_count);
    return (char*)hdr->slow_path[chunk_idx] + (local_idx * hdr->elem_size);
}

void pbuf_debug_dump_slow_chunks(const void *buf) {
    if (!buf) {
        printf("Buffer is NULL (empty).\n");
        return;
    }

    pbufhdr *hdr = _pbufhdr(buf);
    
    printf("=== STRETCHY BUFFER INTERNAL SLOW-PATH DUMP ===\n");
    printf("Total Elements Tracked : %zu\n", hdr->len);
    printf("Total Capacity Allocated: %zu elements\n", hdr->cap);
    printf("Element Type Size       : %zu bytes\n", hdr->elem_size);
    printf("Fast-Path Base Address  : %p\n", (void*)hdr->data);
    printf("Slow-Path Table Capacity: %u chunk pointers\n", hdr->slow_capacity);
    printf("Active Slow-Path Chunks : %u\n", hdr->slow_count);
    
    if (hdr->slow_count == 0) {
        printf("-> No slow path chunks allocated yet. Operating entirely on Fast-Path.\n");
    } else {
        for (uint32_t i = 0; i < hdr->slow_count; i++) {
            void *chunk_address = hdr->slow_path[i];
            
            // Calculate how many items are currently inside this specific chunk
            usize elements_before_this_chunk = SB_CHUNK_SIZE + (i * SB_CHUNK_SIZE);
            usize items_in_this_chunk = 0;
            
            if (hdr->len > elements_before_this_chunk) {
                items_in_this_chunk = hdr->len - elements_before_this_chunk;
                if (items_in_this_chunk > SB_CHUNK_SIZE) {
                    items_in_this_chunk = SB_CHUNK_SIZE; // Chunk is completely filled
                }
            }
            
            printf("  [Chunk %u] Address: %p | Active Items: %zu / %llu\n", 
                   i, chunk_address, items_in_this_chunk, SB_CHUNK_SIZE);
            
            // Optional: If you want to print the actual integer values inside this chunk:
            /*
            int *int_chunk = (int*)chunk_address;
            printf("    First value in Chunk %u: %d\n", i, int_chunk[0]);
            */
        }
    }
    printf("================================================\n\n");
}
