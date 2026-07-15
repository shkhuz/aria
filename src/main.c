#include <stdio.h>

#include "core.h"
#include "compile.h"
#include "tmp.h"

int main() {
    int* a = NULL;
    bufpush(a, 1);
    bufpush(a, 2);
    assert(bufpop(a) == 2);
    assert(bufpop(a) == 1);
    assert(buflen(a) == 0);

    printf("[System] Creating a single 4GB virtual arena slot...\n");
    Arena arena = arena_create(4ULL << 30); // 4 GB space
    if (!arena.base) {
        fprintf(stderr, "Fatal: OS mmap failed to allocate space.\n");
        return 1;
    }

    // Declare 3 independent parallel int arrays
    int *array_a = NULL;
    int *array_b = NULL;
    int *array_c = NULL;

    printf("[Test] Populating initial elements across multiple buffers...\n");
    
    // Interleave pushes to prove independent linear layout tracking
    pbufpush(&arena, array_a, 10);
    pbufpush(&arena, array_b, 100);
    pbufpush(&arena, array_c, 1000);

    pbufpush(&arena, array_a, 20);
    pbufpush(&arena, array_b, 200);
    pbufpush(&arena, array_c, 2000);

    printf("-> Array A (Index 1): %d\n", pbufget(array_a, 1));
    printf("-> Array B (Index 1): %d\n", pbufget(array_b, 1));
    printf("-> Array C (Index 1): %d\n\n", pbufget(array_c, 1));

    // Prove that data-oriented parallel matching works seamlessly
    printf("[Test] Verifying data-oriented parallel mapping...\n");
    pbufloop(array_a, i) {
        printf("   Matching Slot [%zu]: A=%d, B=%d, C=%d\n", 
               i, pbufget(array_a, i), pbufget(array_b, i), pbufget(array_c, i));
    }
    printf("\n");

    printf("[Test] Triggering a massive allocation spike on Buffer B only...\n");
    
    // Force Array B deep into the slow path while keeping A and C tiny
    usize slow_target_idx = SB_CHUNK_SIZE + 999;
    pbuffit(&arena, array_b, slow_target_idx + 1);
    _pbufhdr(array_b)->len = slow_target_idx + 1;
    
    // Write deep inside the slow path of Array B
    pbufget(array_b, slow_target_idx) = 99999;

    // --- NEW VERIFICATION CALLS ---
    printf("\n[Debug] Inspecting Array A (Should be Fast-Path only):\n");
    pbuf_debug_dump_slow_chunks(array_a);

    printf("[Debug] Inspecting Array B (Should show the linked slow-path chunks):\n");
    pbuf_debug_dump_slow_chunks(array_b);

    printf("-> Array B Slow Path Val (Index %zu): %d\n", slow_target_idx, pbufget(array_b, slow_target_idx));
    printf("-> Array A Length (Should remain 2): %zu\n", pbuflen(array_a));
    printf("-> Array B Length (Should be spiked): %zu\n", pbuflen(array_b));
    printf("-> Array C Length (Should remain 2): %zu\n\n", pbuflen(array_c));

    printf("[System] Wiping all memory simultaneously via Arena reset...\n");
    // Detach exposed pointers
    pbuffree(array_a);
    pbuffree(array_b);
    pbuffree(array_c);
    
    // One single pointer change zeroes out all buffers instantly
    arena_clear(&arena); 
    
    printf("[System] Dropping address map.\n");
    arena_destroy(&arena);

    printf("[Success] Multi-buffer test execution complete.\n");
    return 0;




    init_core();
    CompileCtx c = compilectx_new();
    // compilectx_init_stream(
    //     &c, 
    //     "imm a = some;\n"
    //     "a:a,\n"
    //     "hi:struct(\"name\"),\n"
    //     "imm b = struct(\"hiya\");\n"
    // );
    compilectx_init_path(
        &c,
        "examples/v2-1.ar"
    );
    compile(&c);
    stri_print_stats(&c.interner);
    if (c.parsing_error) compile_terminate(&c);
}
