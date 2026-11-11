// heap.c
//
// Linux and macOS. The page heap is one large reservation of address space,
// committed a run of pages at a time as it grows.

// MAP_ANONYMOUS is a glibc extension under a strict -std
#define _DEFAULT_SOURCE

#include "heap.h"

#include <stddef.h>
#include <sys/mman.h>

#ifndef MAP_NORESERVE
#  define MAP_NORESERVE 0
#endif

// Address space is cheap: we only pay for pages once they are committed.
#if UINTPTR_MAX > 0xFFFFFFFFu
#  define RC_PAGE_HEAP_RESERVE ((uint64_t)64 << 30)
#else
#  define RC_PAGE_HEAP_RESERVE ((uint64_t)1 << 30)
#endif

rc_heap rc_heap_reserve(void)
{
    // mmap only promises the OS page size, so over-reserve by one of our pages
    // and align the base up within it
    size_t size = (size_t)RC_PAGE_HEAP_RESERVE;
    size_t page = RC_HEAP_PAGE_SIZE;
    void *mem = mmap(NULL, size + page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (mem == MAP_FAILED) {
        return (rc_heap) {0};
    }

    // Trim the slack either side, so the mapping is exactly the heap and
    // release can unmap it by base and size alone
    uintptr_t start = (uintptr_t)mem;
    uintptr_t base = (start + page - 1) & ~(uintptr_t)(page - 1);
    size_t head = (size_t)(base - start);
    if (head) {
        munmap(mem, head);
    }
    munmap((void *)(base + size), page - head);

    return (rc_heap) {
        .base = base,
        .first = 1,
        .limit = (uint32_t)(RC_PAGE_HEAP_RESERVE / RC_HEAP_PAGE_SIZE)
    };
}


bool rc_heap_commit(uintptr_t base, uint32_t page, uint32_t count)
{
    void *first = (void *)(base + (uintptr_t)page * RC_HEAP_PAGE_SIZE);

    return mprotect(first, (size_t)count * RC_HEAP_PAGE_SIZE, PROT_READ | PROT_WRITE) == 0;
}


void rc_heap_release(uintptr_t base)
{
    munmap((void *)base, (size_t)RC_PAGE_HEAP_RESERVE);
}
