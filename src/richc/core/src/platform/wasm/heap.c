// heap.c
//
// wasm32. The page heap is linear memory itself: wasm pages are our pages, and
// memory.grow appends them at the end.
//
// Linear memory never shrinks, so release keeps everything committed, and the
// next reserve hands the same pages out again. richc must be the only thing
// that grows memory, or the heap would not be contiguous.

#include "heap.h"

#include <stddef.h>

#include "richc/macros.h"

// wasm32 memory tops out at 4 GiB.
enum {
    RC_WASM_MAX_PAGES = 65536
};

static uint32_t rc_wasm_heap_first;   // where memory ended when we first looked
static uint32_t rc_wasm_committed;    // memory size in pages as of our last grow

rc_heap rc_heap_reserve(void)
{
    if (!rc_wasm_heap_first) {
        uint32_t size = (uint32_t)__builtin_wasm_memory_size(0);

        // Static data and the stack fill the front of memory, so this is never
        // 0 in practice; if it were, page 0 would still not be handed out
        rc_wasm_heap_first = size ? size : 1;
        rc_wasm_committed = size;
    }

    return (rc_heap) {
        .base = 0,
        .first = rc_wasm_heap_first,
        .limit = RC_WASM_MAX_PAGES
    };
}


bool rc_heap_commit(uintptr_t base, uint32_t page, uint32_t count)
{
    (void)base;

    // Something else grew memory, so our pages are no longer contiguous
    RC_PANIC(__builtin_wasm_memory_size(0) == rc_wasm_committed);

    uint32_t end = page + count;
    if (end <= rc_wasm_committed) {
        return true;   // committed before the last release
    }
    if (__builtin_wasm_memory_grow(0, end - rc_wasm_committed) == SIZE_MAX) {
        return false;
    }
    rc_wasm_committed = end;
    return true;
}


void rc_heap_release(uintptr_t base)
{
    (void)base;
}
