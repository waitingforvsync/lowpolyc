// heap.h
//
// The page heap's view of memory: reserve address space, commit pages, give it all back. Private to
// core, and implemented once per target in platform/<target>/heap.c, chosen by the build.

#ifndef RC_HEAP_H_
#define RC_HEAP_H_

#include <stdbool.h>
#include <stdint.h>

// The page heap: one contiguous run of 64 KiB pages (the wasm page size), committed in order from
// the front. Page 0 is never handed out, so a page index of 0 can mean "none".
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

// Make count pages from page onwards usable. Each commit starts where the last one ended. False
// when out of memory.
bool rc_heap_commit(uintptr_t base, uint32_t page, uint32_t count);

// Give the whole reservation back; every page in it becomes invalid.
void rc_heap_release(uintptr_t base);

#endif // RC_HEAP_H_
