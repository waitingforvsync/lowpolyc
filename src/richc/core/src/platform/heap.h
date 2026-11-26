// platform/heap.h
//
// The page heap's view of memory: reserve address space, commit pages and give them back, and give
// it all back. Private to core, and implemented once per target in platform/<target>/heap.c, chosen
// by the build. arena.c holds the page heap's lock for every call, so none need be thread-safe.

#ifndef RC_PLATFORM_HEAP_H_
#define RC_PLATFORM_HEAP_H_

#include <stdbool.h>
#include <stdint.h>

// The page heap: one contiguous run of 64 KiB pages (the wasm page size), used from the front. Page
// 0 is never handed out, so a page index of 0 can mean "none".
enum {
    RC_HEAP_PAGE_SIZE = 65536
};

// A reserved range of pages.
typedef struct rc_heap {
    uintptr_t base;   // address of page 0, aligned to RC_HEAP_PAGE_SIZE
    uint32_t first;   // first page the heap may hand out (never 0)
    uint32_t limit;   // one past the last page it may use; 0 if the reserve failed
} rc_heap;

// Reserve address space for the page heap. Nothing is usable until committed.
rc_heap rc_heap_reserve(void);

// Make count (non-zero) pages from page onwards usable. Committing a page that already is must
// work, and keep what it holds. False when out of memory.
bool rc_heap_commit(uintptr_t base, uint32_t page, uint32_t count);

// Give count (non-zero) committed pages from page onwards back to the OS. What they held is lost,
// and they must be committed again before they are used; natively, touching them until then
// faults. Where memory cannot be given back (wasm), this does nothing, and they stay usable.
void rc_heap_decommit(uintptr_t base, uint32_t page, uint32_t count);

// Give the whole reservation back; every page in it becomes invalid.
void rc_heap_release(uintptr_t base);

#endif // RC_PLATFORM_HEAP_H_
