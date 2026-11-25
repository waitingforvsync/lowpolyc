// heap.c
//
// Linux and macOS. The page heap is one large reservation of address space, committed a run of
// pages at a time as it grows, and given back the same way.

// MAP_ANONYMOUS is a glibc extension under a strict -std
#define _DEFAULT_SOURCE

#include "heap.h"

#include <stddef.h>
#include <sys/mman.h>

#ifndef MAP_NORESERVE
#  define MAP_NORESERVE 0
#endif

// The advice that lets the OS have pages back: at once on Linux, and once it needs them on macOS,
// where MADV_DONTNEED does nothing.
#if defined(__APPLE__)
#  define RC_MADV_DECOMMIT MADV_FREE
#else
#  define RC_MADV_DECOMMIT MADV_DONTNEED
#endif

// Address space is cheap: we only pay for pages once they are committed.
#if UINTPTR_MAX > 0xFFFFFFFFu
#  define RC_PAGE_HEAP_RESERVE ((uint64_t)64 << 30)
#else
#  define RC_PAGE_HEAP_RESERVE ((uint64_t)1 << 30)
#endif

// Trim the slack either side of a mapping at start, a page bigger than the heap, so the mapping is
// exactly the heap and release can unmap it by base and size alone.
static rc_heap rc_heap_trim(uintptr_t start)
{
    size_t size = (size_t)RC_PAGE_HEAP_RESERVE;
    size_t page = RC_HEAP_PAGE_SIZE;
    uintptr_t base = (start + page - 1) & ~(uintptr_t)(page - 1);
    size_t head = (size_t)(base - start);

    if (head) {
        munmap((void *)start, head);
    }

    munmap((void *)(base + size), page - head);
    return (rc_heap) {
        .base = base,
        .first = 1,
        .limit = (uint32_t)(RC_PAGE_HEAP_RESERVE / RC_HEAP_PAGE_SIZE)
    };
}


rc_heap rc_heap_reserve(void)
{
    // mmap only promises the OS page size, so over-reserve by one of our pages and align the base
    // up within it
    size_t mapping = (size_t)RC_PAGE_HEAP_RESERVE + RC_HEAP_PAGE_SIZE;
    void *mem = mmap(NULL, mapping, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

    if (mem == MAP_FAILED) {
        return (rc_heap) {0};
    }

    return rc_heap_trim((uintptr_t)mem);
}


bool rc_heap_commit(uintptr_t base, uint32_t page, uint32_t count)
{
    void *first = (void *)(base + (uintptr_t)page * RC_HEAP_PAGE_SIZE);

    return mprotect(first, (size_t)count * RC_HEAP_PAGE_SIZE, PROT_READ | PROT_WRITE) == 0;
}


void rc_heap_decommit(uintptr_t base, uint32_t page, uint32_t count)
{
    void *first = (void *)(base + (uintptr_t)page * RC_HEAP_PAGE_SIZE);
    size_t size = (size_t)count * RC_HEAP_PAGE_SIZE;

    // Taking access away too makes a stray pointer into the pages fault, as it does on Windows. If
    // either call fails, the pages just stay committed.
    madvise(first, size, RC_MADV_DECOMMIT);
    mprotect(first, size, PROT_NONE);
}


void rc_heap_release(uintptr_t base)
{
    munmap((void *)base, (size_t)RC_PAGE_HEAP_RESERVE);
}
