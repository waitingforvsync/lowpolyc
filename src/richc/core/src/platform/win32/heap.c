// heap.c
//
// Windows. The page heap is one large reservation of address space, committed a run of pages at a
// time as it grows.

#include "heap.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Address space is cheap: we only pay for pages once they are committed.
#if UINTPTR_MAX > 0xFFFFFFFFu
#  define RC_PAGE_HEAP_RESERVE ((uint64_t)64 << 30)
#else
#  define RC_PAGE_HEAP_RESERVE ((uint64_t)1 << 30)
#endif

rc_heap rc_heap_reserve(void)
{
    // Reservations are aligned to the 64 KiB allocation granularity, which is exactly our page
    // size, so no over-reserving is needed
    void *mem = VirtualAlloc(NULL, (SIZE_T)RC_PAGE_HEAP_RESERVE, MEM_RESERVE, PAGE_NOACCESS);
    if (!mem) {
        return (rc_heap) {0};
    }

    return (rc_heap) {
        .base = (uintptr_t)mem,
        .first = 1,
        .limit = (uint32_t)(RC_PAGE_HEAP_RESERVE / RC_HEAP_PAGE_SIZE)
    };
}


bool rc_heap_commit(uintptr_t base, uint32_t page, uint32_t count)
{
    void *first = (void *)(base + (uintptr_t)page * RC_HEAP_PAGE_SIZE);

    return VirtualAlloc(first, (SIZE_T)count * RC_HEAP_PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE) != NULL;
}


void rc_heap_release(uintptr_t base)
{
    VirtualFree((void *)base, 0, MEM_RELEASE);
}
