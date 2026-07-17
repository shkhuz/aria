#include "core.h"

usize align_to_pow2(size_t n, size_t pow2) {
    if (pow2 == 0) return n;
    return (n + (pow2-1)) & ~(pow2-1);
}

usize u64_bitlength(u64 n) {
    for (int i = U64_BITS-1; i >= 0; i--) {
        if ((n >> i) & 1) return i+1;
    }
    return 0;
}

usize get_bits_for_value(u128 n) {
    size_t count = 0;
    while (n > 0) {
            n = n >> 1;
            count++;
    }
    return count;
}

int char_to_digit(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    assert(isxdigit(c));
    return tolower(c) - 'a' + 10;
}

bool is_octal_digit(char c) {
    if ('0' <= c && c <= '7') {
        return true;
    }
    return false;
}

u64 maxinteger_unsigned(int bytes) {
    switch (bytes) {
        case 1: return UINT8_MAX;
        case 2: return UINT16_MAX;
        case 4: return UINT32_MAX;
        case 8: return UINT64_MAX;
        default: assert(0 && "maxinteger: Invalid byte count"); break;
    }
}

u64 maxinteger_signed(int bytes) {
    switch (bytes) {
        case 1: return INT8_MAX;
        case 2: return INT16_MAX;
        case 4: return INT32_MAX;
        case 8: return INT64_MAX;
        default: assert(0 && "maxinteger: Invalid byte count"); break;
    }
}

bool slice_eql_to_str(const char* slice, int slicelen, const char* str) {
    bool equal = true;
    const char* c = str;
    while (*c != '\0') {
        if (c-str == slicelen) {
            equal = false;
            break;
        }
        if (*c != slice[c-str]) {
            equal = false;
            break;
        }
        c++;
    }
    if (c-str != slicelen) equal = false;
    return equal;
}

char* format_string(const char* fmt, ...) {
    char* buf;
    va_list args;
    va_start(args, fmt);
    vasprintf(&buf, fmt, args);
    va_end(args);
    return buf;
}

u32 hash_string(const char* str, usize len) {
    u32 hash = 2166136261u;
    for (usize i = 0; i < len; i++) {
        hash ^= (u8)str[i];
        hash *= 16777619u;
    }
    return hash;
}

usize get_memory_usage() {
    usize physical_pages = 0;
    FILE* fp = fopen("/proc/self/statm", "r");
    if (fp) {
        usize total_vm_pages = 0;
        if (fscanf(
            fp, 
            "%ld %ld", 
            &total_vm_pages, 
            &physical_pages
        ) != 2) {
            physical_pages = 0;
        }

        fclose(fp);
    }

    usize page_size = sysconf(_SC_PAGESIZE);
    usize physical_bytes = physical_pages * page_size;
    return physical_bytes;
}

usize print_memory_size(usize bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = (double)bytes;

    while (size >= 1024 && unit_index < 4) {
        size /= 1024;
        unit_index++;
    }

    if (unit_index == 0) {
        printf("%.0f %s", size, units[unit_index]);
    } else {
        double frac = fabs(size - (int)size);
        if (frac < 0.01) {
            printf("%.0f %s", size, units[unit_index]);
        } else {
            printf("%.2f %s", size, units[unit_index]);
        }
    }
    return bytes;
}

// =============================================================================
// ARENA
// =============================================================================

Arena arena_create(const char* name, u64 reserve) {
    Arena arena = (Arena){};
    arena.name = name;
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


void* arena_push(Arena* arena, u64 size, const char* reason) {
    u64 aligned_size = (size + 7) & ~7;
    assert(arena->pos + aligned_size <= arena->capacity && "Arena out of memory!");
    // printf("\n[I] arena_push(%s, ", arena->name);
    // print_memory_size(aligned_size);
    // printf(", %s) = ", reason ? reason : "");
    void* ptr = arena->base + arena->pos;
    arena->pos += aligned_size;
    // print_memory_size(arena->pos);
    return ptr;
}

void arena_print_segment_metrics(const Arena *arena) {
    if (!arena || !arena->base) {
        printf("Arena '%s' is uninitialized.\n", arena->name);
        return;
    }

    FILE* fp = fopen("/proc/self/smaps", "r");
    if (!fp) {
        perror("Failed to open /proc/self/smaps");
        return;
    }

    char line[512];
    uintptr_t target_addr = (uintptr_t)arena->base;
    int inside_target_segment = 0;
    u64 size = 0, rss = 0, anon = 0, thp = 0;

    while (fgets(line, sizeof(line), fp)) {
        uintptr_t start = 0, end = 0;
        if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
            if (target_addr >= start && target_addr < end) {
                inside_target_segment = 1;
                printf("Arena %s [%p] \n  Mapped Region: %lx-%lx\n", 
                       arena->name, (void*)target_addr, start, end);
            } else {
                inside_target_segment = 0;
            }
            continue;
        }

        if (inside_target_segment) {
            if (strncmp(line, "Size:", 5) == 0)          sscanf(line, "Size: %lu", &size);
            else if (strncmp(line, "Rss:", 4) == 0)      sscanf(line, "Rss: %lu", &rss);
            else if (strncmp(line, "Anonymous:", 10) == 0) sscanf(line, "Anonymous: %lu", &anon);
            else if (strncmp(line, "AnonHugePages:", 14) == 0) {
                sscanf(line, "AnonHugePages: %lu", &thp);
                
                printf("  Segment Virtual Limit    : "); print_memory_size(size*1024);
                printf("\n  Physical Memory Committed: "); print_memory_size(rss*1024);
                printf("\n  Anonymous Allocation     : "); print_memory_size(anon*1024);
                printf("\n  Transparent Huge Pages   : "); print_memory_size(thp*1024);
                printf("\n");
                break; 
            }
        }
    }

    fclose(fp);
}

// =============================================================================
// BUFFERS
// =============================================================================

usize buflen(const void* buf) {
    return buf ? _bufhdr(buf)->len : 0;
}

usize bufcap(const void* buf) {
    return buf ? _bufhdr(buf)->cap : 0;
}

void* _bufgrow(const void* buf, usize new_len, usize elem_size) {
    usize new_cap = CLAMP_MIN(2 * bufcap(buf), MAX(new_len, 16));
    assert(new_len <= new_cap);

    usize mem_to_alloc = new_cap * elem_size + offsetof(bufhdr, data);
    bufhdr* new_hdr;
    if (buf) {
        new_hdr = (bufhdr*)xrealloc(_bufhdr(buf), mem_to_alloc);
    } else {
        new_hdr = (bufhdr*)xmalloc(mem_to_alloc);
        new_hdr->len = 0;
    }

    new_hdr->cap = new_cap;
    return new_hdr->data;
}

// =============================================================================
// LIST
// =============================================================================

usize listlen(const void* list) {
    if (list) return _listhdr(list)->len;
    assert(0);
    return 0;
}

usize listcap(const void* list) {
    if (list) return _listhdr(list)->cap;
    assert(0);
    return 0;
}

listhdr* _listhdr(const void* list) { 
    return (listhdr*)((char*)list - sizeof(listhdr)); 
}

void* _listgrow(Arena* arena, const void* list, usize new_len, usize elem_size) {
    listhdr* hdr = list ? _listhdr(list) : NULL;
    if (!hdr) {
        hdr = (listhdr*)arena_push(arena, sizeof(listhdr), "_listgrow: hdr");
        hdr->arena = arena;
        hdr->len = 0;
        hdr->cap = 0;
        hdr->chunkcap = 0;
        hdr->chunkcount = 0;
        hdr->chunks = NULL;
    }

    while (new_len > hdr->cap) {
        if (hdr->chunkcount >= hdr->chunkcap) {
            u32 oldcap = hdr->chunkcap;
            hdr->chunkcap = hdr->chunkcap == 0 ? 4 : hdr->chunkcap * 2;
            void** newchunks = arena_push(hdr->arena, hdr->chunkcap*sizeof(void*), "_listgrow: ptrs");
            if (oldcap > 0) memcpy(newchunks, hdr->chunks, oldcap*sizeof(void*));
            hdr->chunks = newchunks;
        }
        hdr->cap += LIST_CHUNK_SIZE;
        hdr->chunks[hdr->chunkcount] = arena_push(hdr->arena, LIST_CHUNK_SIZE * elem_size, "_listgrow: chunk");
        hdr->chunkcount++;
    }
    return (void*)((char*)hdr + sizeof(listhdr));
}

void list_dump_chunks(const char* name, const void *list) {
    if (!list) {
        return;
    }

    listhdr *hdr = _listhdr(list);
    printf("Dumping list %s (%p) info: \n", name ? name : "", list);
    printf("  Total Elements Tracked : %zu\n", hdr->len);
    printf("  Total Capacity Allocated: %zu elements\n", hdr->cap);
    printf("  Pointer Table Capacity : %u slots\n", hdr->chunkcap);
    printf("  Active Uniform Blocks  : %u\n", hdr->chunkcount);
    
    for (u32 i = 0; i < hdr->chunkcount; i++) {
        void* block_address = hdr->chunks[i];
        
        usize start_idx = i * LIST_CHUNK_SIZE;
        usize end_idx   = start_idx + LIST_CHUNK_SIZE - 1;
        
        printf("    [Block %u] Address: %p [%zu to %zu]\n", 
               i, block_address, start_idx, end_idx);
    }
}

// =============================================================================
// STRING INTERNING
// =============================================================================

#define STRI_INIT_MAP_CAP 16
#define STRI_LOAD_FACTOR 0.75

static void stri_clear_buckets(stri* s, usize start, usize end) {
    u32(*buckets)() = s->buckets;
    for (usize i = start; i < end; i++) {
        listget(buckets, i) = STRI_INVALID_ID;
    }
}

void stri_init(Arena* arena, stri* s) {
    s->slices = NULL;
    s->buckets = NULL;
    s->nodes = NULL;
    listinit(arena, s->slices);
    listinit(arena, s->buckets);
    listinit(arena, s->nodes);

    listfit(s->buckets, STRI_INIT_MAP_CAP);
    _listhdr(s->buckets)->len = STRI_INIT_MAP_CAP;
    stri_clear_buckets(s, 0, STRI_INIT_MAP_CAP);
}

static void stri_resize_buckets(stri* s) {
    usize oldcap = listlen(s->buckets);
    usize newcap = oldcap * 2;
    listfit(s->buckets, newcap);
    _listhdr(s->buckets)->len = newcap;
    stri_clear_buckets(s, 0, newcap);

    for (usize i = 0; i < listlen(s->nodes); i++) {
        u32 newbucket = listget(s->nodes, i).hash % newcap;
        listget(s->nodes, i).next_nodeid = listget(s->buckets, newbucket);
        listget(s->buckets, newbucket) = (u32)i;
    }
}

strid stri_intern(stri* s, const char* str, usize len) {
    u32 hash = hash_string(str, len);
    usize bucketcap = listlen(s->buckets);
    u32 bucketid = hash % bucketcap;

    u32 nodeid = listget(s->buckets, bucketid);
    while (nodeid != STRI_INVALID_ID) {
        strinode node = listget(s->nodes, nodeid);
        if (node.hash == hash) {
            strislice ent = listget(s->slices, node.id);
            if (ent.len == len 
                    && strncmp(ent.ptr, str, len) == 0) {
                return node.id;
            }
        }
        nodeid = node.next_nodeid;
    }

    if ((float)(listlen(s->nodes)+1) / (float)bucketcap 
            > STRI_LOAD_FACTOR) {
        stri_resize_buckets(s);
        bucketcap = listlen(s->buckets);
        bucketid = hash % bucketcap;
    }

    strid newstrid = (strid)listlen(s->slices);
    strislice newslice = (strislice){.ptr = str, .len = len};
    listpush(s->slices, newslice);

    strinode newnode = (strinode){
        .hash = hash,
        .id = newstrid,
        .next_nodeid = listget(s->buckets, bucketid)
    };
    u32 newnodeid = (u32)listlen(s->nodes);
    listpush(s->nodes, newnode);
    listget(s->buckets, bucketid) = newnodeid;
    return newstrid;
}

strislice stri_lookup(const stri* s, strid id) {
    return listget(s->slices, id);
}

void stri_print_stats(const stri* s) {
    usize total_nodes = 0;
    usize active_buckets = 0;
    usize collided_nodes = 0;
    float mean_collision_ratio = 0;
    usize max_chainlen = 0;

    total_nodes = listlen(s->nodes);
    usize bucketcap = listlen(s->buckets);
    for (usize i = 0; i < bucketcap; i++) {
        uint32_t nodeid = listget(s->buckets, i);
        if (nodeid == STRI_INVALID_ID) continue;
        active_buckets++;
        
        usize chainlen = 0;
        while (nodeid != STRI_INVALID_ID) {
            chainlen++;
            nodeid = listget(s->nodes, nodeid).next_nodeid;
        }
        
        if (chainlen > max_chainlen) {
            max_chainlen = chainlen;
        }
    }

    if (total_nodes != 0) {
        collided_nodes = total_nodes - active_buckets;
        mean_collision_ratio = (float)collided_nodes / (float)total_nodes;
    }

    printf("=== STRING INTERNER STATS ===\n");
    printf("Unique Strings Saved     : %zu\n", listlen(s->slices));
    printf("Total Nodes Registered   : %zu\n", listlen(s->nodes));
    printf("Final Buckets Capacity   : %zu\n", listlen(s->buckets));
    printf("Total Active Buckets     : %zu / %zu\n", active_buckets, listlen(s->buckets));
    printf("Total Collided Nodes     : %zu\n", collided_nodes);
    printf("Mean Collision Ratio     : %.2f%%\n", mean_collision_ratio * 100.0f);
    printf("Max Chain Depth Length   : %zu\n", max_chainlen);
    printf("============================\n");
}

// =============================================================================
// FILE IO
// =============================================================================

bool file_exists(const char* path) {
    return access(path, F_OK) == 0;
}

int is_dir(const char* path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

FileOrError read_file(Arena* arena, const char* path) {
    // TODO: more thorough error checking
    FILE* raw = fopen(path, "r");
    if (!raw) {
        return (FileOrError){ {}, FILEIO_FAILURE };
    }

    if (is_dir(path)) {
        return (FileOrError){ {}, FILEIO_DIRECTORY };
    }

    fseek(raw, 0, SEEK_END);
    usize size = ftell(raw);
    rewind(raw);

    char* contents = (char*)arena_push(arena, size + 1, "readfile: source code");
    fread(contents, sizeof(char), size, raw);
    fclose(raw);
    contents[size] = '\0';

    char abs_path_buf[PATH_MAX + 1];
    char* abs_path = NULL;
    if (realpath(path, abs_path_buf)) {
        usize abs_path_len = strlen(abs_path_buf);
        abs_path = (char*)arena_push(arena, abs_path_len + 1, "abs path");
        // including '\0'
        memcpy(abs_path, abs_path_buf, abs_path_len+1); 
    }

    File handle;
    handle.path = strdup(path);
    handle.abs_path = abs_path;
    handle.contents = contents;
    handle.len = size;
    return (FileOrError){ handle, FILEIO_SUCCESS };
}

FileOpResult write_bin_file(
    const char* path, 
    const char* contents, 
    u64 bytes
) {
    FILE* raw = fopen(path, "w");
    if (!raw) {
        return FILEIO_FAILURE;
    }

    if (is_dir(path)) {
        return FILEIO_DIRECTORY;
    }

    fwrite(contents, bytes, 1, raw);
    fclose(raw);
    return FILEIO_SUCCESS;
}

const char* file_get_line_ptr(const File* handle, usize line) {
    const char* ptr = handle->contents;
    usize l = 1;
    while (l != line) {
        while (*ptr++ != '\n') {
            if (*ptr == '\0') return NULL;
        }
        l++;
    }
    return ptr;
}

// =============================================================================
// CLI COLORS
// =============================================================================

char* g_green_color = EC_8BITCOLOR("3", "0");
char* g_bold_green_color = EC_8BITCOLOR("3", "1");
char* g_red_color = EC_8BITCOLOR("196", "0");
char* g_bold_red_color = EC_8BITCOLOR("196", "1");
char* g_error_color = EC_8BITCOLOR("196", "1");
char* g_warning_color = EC_8BITCOLOR("184", "1"); // 208
char* g_note_color = "\x1B[1m";
char* g_bold_color = "\x1B[0;1m";
char* g_bold_grey_color = EC_8BITCOLOR("244", "1");
char* g_grey_color = EC_8BITCOLOR("247", "0");
char* g_reset_color = "\x1B[0m";
char* g_bold_cornflower_blue_color = EC_8BITCOLOR("69", "1");

// =============================================================================
// BIGINT
// =============================================================================

// This biginteger implementation was "inspired" (more like copied) from
// the fantastic 983/Num bigint library. Thanks 983!
// Source: https://github.com/983/Num

#define WORD_HALF_MASK 0x00000000ffffffff

bigint BIGINT_ZERO;

bigint bigint_new() {
    bigint b;
    b.d = NULL;
    b.neg = false;
    return b;
}

bigint bigint_new_u64(u64 num) {
    bigint b = bigint_new();
    bufpush(b.d, num);
    bigint_normalize(&b);
    return b;
}

void bigint_clear(bigint* a) {
    bufclear(a->d);
    a->neg = false;
}

void bigint_normalize(bigint* a) {
    while (buflen(a->d) > 0 && *buflast(a->d) == 0) {
        bufpop(a->d);
    }
    if (buflen(a->d) == 0) a->neg = false;
}

void bigint_set_u64(bigint* a, u64 num) {
    bigint_clear(a);
    bufpush(a->d, num);
    bigint_normalize(a);
}

usize bigint_bitlength(const bigint* a) {
    if (buflen(a->d) == 0) return 0;
    usize msw_idx = buflen(a->d)-1;
    return u64_bitlength(a->d[msw_idx]) + msw_idx*U64_BITS;
}

void bigint_copy(bigint* dest, const bigint* src) {
    bigint_clear(dest);
    bufloop(src->d, i) {
        bufpush(dest->d, src->d[i]);
    }
    dest->neg = src->neg;
    bigint_normalize(dest);
}

void bigint_free(bigint* a) {
    buffree(a->d);
}

int bigint_cmp_abs(const bigint* a, const bigint* b) {
    usize na = buflen(a->d);
    usize nb = buflen(b->d);
    if (na == nb) {
        for (usize i = na; i --> 0;) {
            if (a->d[i] != b->d[i]) {
                return a->d[i] > b->d[i] ? 1 : -1;
            }
        }
        return 0;
    } else {
        return na > nb ? 1 : -1;
    }
}

int bigint_cmp(const bigint* a, const bigint* b) {
    if (buflen(a->d) == 0 && buflen(b->d) == 0) return 0;
    else if (!a->neg && !b->neg) return  bigint_cmp_abs(a, b);
    else if ( a->neg &&  b->neg) return -bigint_cmp_abs(a, b);
    else return !a->neg && b->neg ? 1 : -1;
}

void bigint_neg(bigint* a) {
    a->neg = !a->neg;
}

static u64 add_wcarry(u64* a, u64 b) {
    *a += b;
    return *a < b;
}

static u64 sub_wcarry(u64* a, u64 b) {
    u64 tmp = *a;
    *a -= b;
    return *a > tmp;
}

// Only "adds" two bigints (doesn't take into account the `neg` flag).
void bigint_add_unsigned(bigint* a, const bigint* b) {
    usize na = buflen(a->d);
    usize nb = buflen(b->d);
    usize n = MAX(na, nb);
    while (buflen(a->d) != n) bufpush(a->d, 0);
    u64 carry = 0;

    usize i;
    for (i = 0; i < nb; i++) {
        carry =  add_wcarry(&a->d[i], carry);
        carry += add_wcarry(&a->d[i], b->d[i]);
    }
    for (; i < na && carry; i++) {
        carry = add_wcarry(&a->d[i], carry);
    }
    if (carry) bufpush(a->d, carry);
    bigint_normalize(a);
}

void bigint_sub_unsigned(bigint* a, const bigint* b) {
    u64 carry = 0;
    usize i;
    for (i = 0; i < buflen(b->d); i++) {
        carry =  sub_wcarry(&a->d[i], carry);
        carry += sub_wcarry(&a->d[i], b->d[i]);
    }
    for (; i < buflen(a->d) && carry; i++) {
        carry = sub_wcarry(&a->d[i], carry);
    }
    bigint_normalize(a);
}

void bigint_add_signed(bigint* a, bool aneg, const bigint* b, bool bneg) {
    if (aneg == bneg) {
        bigint_add_unsigned(a, b);
        a->neg = aneg;
    } else if (bigint_cmp_abs(a, b) >= 0) {
        bigint_sub_unsigned(a, b);
        a->neg = aneg;
    } else {
        bigint c = bigint_new();
        bigint_copy(&c, b);
        bigint_sub_unsigned(&c, a);
        bigint_free(a);
        *a = c;
    }
}

void bigint_add(bigint* a, const bigint* b) {
    bigint_add_signed(a, a->neg, b, b->neg);
}

void bigint_sub(bigint* a, const bigint* b) {
    bigint_add_signed(a, a->neg, b, !b->neg);
}

void bigint_shl(bigint* a) {
    u64 carry = 0;
    bufloop(a->d, i) {
        u64 word = a->d[i];
        u64 newcarry = (word & ((u64)1<<63)) >> 63;
        word <<= 1;
        word |= carry;
        carry = newcarry;
        a->d[i] = word;
    }
    if (carry) bufpush(a->d, carry);
    bigint_normalize(a);
}

void bigint_shln(bigint* a, usize n) {
    for (usize i = 0; i < n; i++) bigint_shl(a);
}

void bigint_shr(bigint* a) {
    u64 carry = 0;
    bufrevloop(a->d, i) {
        u64 word = a->d[i];
        u64 newcarry = word & 1;
        word >>= 1;
        word |= (carry<<63);
        carry = newcarry;
        a->d[i] = word;
    }
    bigint_normalize(a);
}

void bigint_shrn(bigint* a, usize n) {
    for (usize i = 0; i < n; i++) bigint_shr(a);
}

void bigint_set_bit(bigint* a, usize bit, bool set) {
    usize word_idx = bit / U64_BITS;
    usize bit_idx_in_word = bit % U64_BITS;

    while (buflen(a->d) < word_idx+1) bufpush(a->d, 0);
    u64 word = a->d[word_idx];
    u64 val = set ? 1 : 0;
    word |= val<<bit_idx_in_word;
    a->d[word_idx] = word;
    bigint_normalize(a);
}

static u64 word_mul_hi(u64 a, u64 b) {
    usize n = U64_BITS/2;
    u64 ahi = a >> n;
    u64 alo = a & WORD_HALF_MASK;
    u64 bhi = b >> n;
    u64 blo = b & WORD_HALF_MASK;
    u64 tmp = ((alo*blo) >> n) + ahi*blo;
    tmp = (tmp >> n) + ((alo*bhi + (tmp & WORD_HALF_MASK)) >> n);
    return tmp + ahi*bhi;
}

void bigint_mul(bigint* a, const bigint* b) {
    usize na = buflen(a->d);
    usize nb = buflen(b->d);
    usize nc = na + nb + 1;

    bigint c = bigint_new();
    c.neg = (int)a->neg ^ (int)b->neg;
    while (buflen(c.d) != nc) {
        bufpush(c.d, 0);
    }

    bigint carries = bigint_new();
    while (buflen(carries.d) != nc) {
        bufpush(carries.d, 0);
    }

    for (usize ia = 0; ia < na; ia++) {
        for (usize ib = 0; ib < nb; ib++) {
            usize i = ia + ib;
            usize j = i + 1;
            carries.d[i+1] += add_wcarry(&c.d[i], a->d[ia] * b->d[ib]);
            carries.d[j+1] += add_wcarry(
                &c.d[j],
                word_mul_hi(a->d[ia], b->d[ib]));
        }
    }
    bigint_add_unsigned(&c, &carries);

    bigint_free(&carries);
    bigint_free(a);
    *a = c;
    bigint_normalize(a);
}

void bigint_div_mod(const bigint* num, const bigint* den, bigint* quo, bigint* rem) {
    // TODO: error on division by zero
    bigint_copy(rem, num);
    bigint_set_u64(quo, 0);

    if (bigint_cmp_abs(rem, den) >= 0) {
        bigint ourden = bigint_new();
        bigint_copy(&ourden, den);

        int n = (int)(bigint_bitlength(rem) - bigint_bitlength(&ourden));
        bigint_shln(&ourden, (usize)n);

        for (; n >= 0; n--) {
            if (bigint_cmp_abs(rem, &ourden) >= 0) {
                bigint_sub_unsigned(rem, &ourden);
                bigint_set_bit(quo, n, true);
            }
            bigint_shr(&ourden);
        }

        bigint_free(&ourden);
    }
    quo->neg = (int)num->neg ^ (int)den->neg;
    rem->neg = rem->neg;

    bigint_normalize(quo);
    bigint_normalize(rem);
}

bool bigint_fits(const bigint* a, int bytes, bool signd) {
    if ((!signd && a->neg) || buflen(a->d) > 1) return false;
    if (buflen(a->d) == 0) return true;

    if (signd) {
        u64 max = maxinteger_signed(bytes);
        if (a->neg) {
            if (a->d[0] > max+1) return false;
            else return true;
        } else {
            if (a->d[0] > max) return false;
            else return true;
        }
    } else {
        if (u64_bitlength(a->d[0]) > (u64)(bytes*8)) return false;
        else return true;
    }
}

char* bigint_tostring(const bigint* a) {
    char* str = NULL;
    if (buflen(a->d) == 0) {
        bufpush(str, '0');
    } else {
        bigint tmp = bigint_new();
        bigint_copy(&tmp, a);
        bigint quo = bigint_new();
        bigint rem = bigint_new();
        bigint base = bigint_new_u64(10);
        char* alpha = "0123456789abcdef";

        while (buflen(tmp.d) > 0) {
            bigint_div_mod(&tmp, &base, &quo, &rem);
            bigint_copy(&tmp, &quo);
            bufpush(str, alpha[rem.d[0]]);
        }
        if (a->neg) bufpush(str, '-');

        usize len = buflen(str);
        usize halflen = len/2;
        for (usize i = 0; i < halflen; i++) {
            char c = str[len-1-i];
            str[len-1-i] = str[i];
            str[i] = c;
        }
    }

    bufpush(str, '\0');
    return str;
}

void test_bigint() {
    /*
    {
        bigint a = bigint_new_u64(UINT64_MAX);
        bigint_shln(&a, 3);
        bigint b = bigint_new_u64(50);
        bigint quo = bigint_new();
        bigint rem = bigint_new();
        bigint_div_mod(&a, &b, &quo, &rem);

        bigint ass = bigint_new_u64(2951479051793528258);
        assert(bigint_cmp_abs(&ass, &quo) == 0);
    }

    {
        bigint new = bigint_new();
        bufpush(new.d, 849348);
        bufpush(new.d, 0);
        bufpush(new.d, 0);
        bufpush(new.d, 0);
        bigint_normalize(&new);
        assert(buflen(new.d) == 1);
    }

    {
        bigint a = bigint_new_u64(UINT64_MAX);
        bigint b = bigint_new_u64(2);
        bigint_mul(&a, &b);
    }

    {
        bigint a = bigint_new_u64(UINT64_MAX);
        const bigint b = bigint_new_u64(UINT64_MAX);
        bigint_mul(&a, &b);
        bigint_mul(&a, &b);
        fprintf(stderr, "num: %s\n", bigint_tostring(&a));
    }

    {
        bigint a = bigint_new_u64(UINT64_MAX);
        assert(bigint_fits(&a, 8, false));
        a = bigint_new_u64(65536);
        assert(!bigint_fits(&a, 2, false));
        assert(bigint_fits(&a, 4, false));
        a = bigint_new_u64(128);
        a.neg = true;
        assert(bigint_fits(&a, 1, true));
    }
    */

    {
        bigint a = bigint_new();
        bufpush(a.d, 0b1001011101001100001011010111100000000000000000000000000000000000);
        bufpush(a.d, 0b1000001000000000000100011100011011000001010010110010111010);

        bigint b = bigint_new_u64(3);

        bigint quo = bigint_new();
        bigint rem = bigint_new();

        bigint_div_mod(&a, &b, &quo, &rem);
        assert(strcmp("900000000000000000000000000000000000", bigint_tostring(&quo)) == 0);
        assert(strcmp("0", bigint_tostring(&rem)) == 0);
    }

    {
        bigint a = bigint_new_u64(7);
        bigint b = bigint_new_u64(3);
        bigint_sub(&a, &b);
        assert(a.d[0] == 4);
    }
}

void init_core() {
    if (!isatty(2)) {
        g_green_color = "";
        g_bold_green_color = "";
        g_red_color = "";
        g_bold_red_color = "";
        g_error_color = "";
        g_warning_color = "";
        g_note_color = "";
        g_bold_color = "";
        g_bold_grey_color = "";
        g_grey_color = "";
        g_reset_color = "";
        g_bold_cornflower_blue_color = "";
    }
    //BIGINT_ZERO = bigint_new_u64(0);
    atexit(print_mem_stats);
}

// =============================================================================
// MEM_STATS
// =============================================================================

static usize curalloc = 0;
static usize peakalloc = 0;

static inline void* get_raw_ptr(void* user_ptr) {
    return (char*)user_ptr - sizeof(usize);
}

static inline usize get_block_size(void* user_ptr) {
    return *(usize*)get_raw_ptr(user_ptr);
}

void* tracked_malloc(
    usize size, 
    const char* file, 
    int line
) {
    usize total_size = size + sizeof(usize);
    void* raw_ptr = malloc(total_size);
    printf("malloc(%lu) at %s,%d\n", size, file, line);
    
    if (!raw_ptr) {
        fprintf(stderr, "[MEM ERROR] Out of memory at %s:%d\n", file, line);
        return NULL;
    }

    *(usize*)raw_ptr = size;
    void* user_ptr = (char*)raw_ptr + sizeof(usize);

    curalloc += size;
    if (curalloc > peakalloc) {
        peakalloc = curalloc;
    }

    return user_ptr;
}

void tracked_free(void* user_ptr) {
    if (!user_ptr) return;

    void* raw_ptr = get_raw_ptr(user_ptr);
    usize size = *(usize*)raw_ptr;

    curalloc -= size;
    free(raw_ptr);
}

void* tracked_realloc(
    void* user_ptr, 
    usize new_size, 
    const char* file, 
    int line
) {
    if (!user_ptr) {
        return tracked_malloc(new_size, file, line);
    } else if (new_size == 0) {
        tracked_free(user_ptr);
        return NULL;
    }

    void* old_raw_ptr = get_raw_ptr(user_ptr);
    usize old_size = *(usize*)old_raw_ptr;

    usize new_total_size = new_size + sizeof(usize);
    void* new_raw_ptr = realloc(old_raw_ptr, new_total_size);

    if (!new_raw_ptr) {
        fprintf(stderr, "[MEM ERROR] Realloc failed at %s:%d\n", file, line);
        return NULL;
    }

    *(usize*)new_raw_ptr = new_size;
    void* new_user_ptr = (char*)new_raw_ptr + sizeof(usize);

    curalloc = (curalloc - old_size) + new_size;
    
    if (curalloc > peakalloc) {
        peakalloc = curalloc;
    }

    return new_user_ptr;
}

void* tracked_calloc(
    usize num, 
    usize size, 
    const char* file, 
    int line
) {
    usize total_bytes = num * size;
    void* user_ptr = tracked_malloc(total_bytes, file, line);
    if (user_ptr) {
        char* p = (char*)user_ptr;
        for (usize i = 0; i < total_bytes; i++) p[i] = 0;
    }
    return user_ptr;
}

void print_mem_stats() {
    printf("Current Leaked Memory: %zu bytes\n", curalloc);
    printf("Peak Memory Footprint: %zu bytes\n", peakalloc);
}
