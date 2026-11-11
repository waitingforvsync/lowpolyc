// arena.c
//
// Chunked arenas over the global page heap, plus the pool of chunks that release hands back.

#include "richc/arena.h"

#include <stddef.h>

#include "heap.h"
#include "richc/mem.h"

enum {
    RC_PAGE = RC_HEAP_PAGE_SIZE,
    RC_HEADER = RC_MAX_ALIGN,           // chunk header size, so allocations start aligned
    RC_MAX_CHUNK_PAGES = 0xFFFF         // keeps a chunk's size in bytes within uint32_t
};

// Largest single allocation: a maximal chunk, less its header.
#define RC_MAX_ALLOC ((uint32_t)RC_MAX_CHUNK_PAGES * RC_PAGE - RC_HEADER)

// XORed with the chunk's own page index, so a stray or misplaced header fails too.
#define RC_CHUNK_SENTINEL 0x52434348u

// Lives at the start of every chunk. Chunks sit end to end in the heap, so an overrun off the end
// of one lands on the next one's sentinel.
typedef struct rc_chunk_header {
    uint32_t sentinel;
    uint32_t next;       // next chunk in an arena's chain, or in the pool
    uint32_t pages;
    uint32_t min_pages;  // growth size, read from an arena's first chunk only
} rc_chunk_header;

RC_STATIC_ASSERT(sizeof(rc_chunk_header) == RC_HEADER);
RC_STATIC_ASSERT(RC_ARENA_MAX_ALIGN == RC_PAGE);
#if !defined(_MSC_VER) || defined(__clang__)
RC_STATIC_ASSERT(alignof(max_align_t) <= RC_MAX_ALIGN);
#endif

// The global page heap. Pages from the first one up to used_pages are committed, and each belongs
// to a chunk, in an arena or in the pool.
typedef struct rc_page_state {
    uintptr_t base;            // address of page 0
    uint32_t used_pages;       // one past the newest page handed out
    uint32_t reserved_pages;   // one past the last page we may use; 0 until init
    uint32_t pool;             // released chunks, linked through their headers
} rc_page_state;

static rc_page_state rc_mem;
static rc_arena_oom_fn *rc_oom_handler;

// ---- private helpers ----

static uint8_t *rc_page_ptr(uint32_t page)
{
    return (uint8_t *)(rc_mem.base + (uintptr_t)page * RC_PAGE);
}


// Every header read comes through here, so every read checks the sentinel.
static rc_chunk_header *rc_chunk(uint32_t page)
{
    rc_chunk_header *h = (rc_chunk_header *)rc_page_ptr(page);

    RC_ASSERT(page != 0);
    RC_ASSERT(h->sentinel == (RC_CHUNK_SENTINEL ^ page));   // something overran the chunk below
    return h;
}


static uint32_t rc_chunk_bytes(uint32_t page)
{
    return rc_chunk(page)->pages * (uint32_t)RC_PAGE;
}


// Callers keep n + align - 1 within uint32_t.
static uint32_t rc_align_up(uint32_t n, uint32_t align)
{
    return (n + align - 1) & ~(align - 1);
}


// Pages needed to hold bytes (which includes the header).
static uint32_t rc_pages_for(uint32_t bytes)
{
    return (uint32_t)(((uint64_t)bytes + RC_PAGE - 1) / RC_PAGE);
}


RC_NORETURN static void rc_arena_out_of_memory(uint32_t size)
{
    if (rc_oom_handler) {
        rc_oom_handler(size);
    }
    RC_PANIC(false);
}


// Commit count more pages at the end of the heap. Returns the first, or 0 when out of memory.
static uint32_t rc_pages_grow(uint32_t count)
{
    if (count > rc_mem.reserved_pages - rc_mem.used_pages) {
        return 0;
    }
    if (!rc_heap_commit(rc_mem.base, rc_mem.used_pages, count)) {
        return 0;
    }

    uint32_t page = rc_mem.used_pages;
    rc_mem.used_pages += count;
    return page;
}


static void rc_chunk_init(uint32_t page, uint32_t pages)
{
    *(rc_chunk_header *)rc_page_ptr(page) = (rc_chunk_header) {
        .sentinel = RC_CHUNK_SENTINEL ^ page,
        .next = 0,
        .pages = pages,
        .min_pages = 0
    };
}


static void rc_pool_push(uint32_t page)
{
    rc_chunk(page)->next = rc_mem.pool;
    rc_mem.pool = page;
}


// A chunk of at least pages pages: the best fit from the pool, else fresh from the heap. 0 when out
// of memory.
static uint32_t rc_chunk_acquire(uint32_t pages)
{
    uint32_t *best = NULL;

    // Best fit from the pool, stopping early at an exact match
    for (uint32_t *link = &rc_mem.pool; *link; link = &rc_chunk(*link)->next) {
        uint32_t have = rc_chunk(*link)->pages;
        if (have >= pages && (!best || have < rc_chunk(*best)->pages)) {
            best = link;
            if (have == pages) {
                break;
            }
        }
    }

    if (best) {
        uint32_t page = *best;
        *best = rc_chunk(page)->next;
        rc_chunk(page)->next = 0;
        return page;
    }

    // Otherwise grow the heap
    uint32_t page = rc_pages_grow(pages);
    if (page) {
        rc_chunk_init(page, pages);
    }
    return page;
}


static bool rc_chunk_fits(uint32_t page, uint32_t need)
{
    return need <= rc_chunk_bytes(page) - RC_HEADER;
}


static void rc_arena_enter_chunk(rc_arena *a, uint32_t page)
{
    a->chunk = page;
    a->top = RC_HEADER;
    a->end = rc_chunk_bytes(page);
}


// Grow the current chunk in place so need more bytes fit after top. Only the newest chunk in the
// heap can grow, since the pages after it are free.
static bool rc_arena_extend(rc_arena *a, uint32_t need)
{
    rc_chunk_header *h = rc_chunk(a->chunk);

    if (a->chunk + h->pages != rc_mem.used_pages) {
        return false;
    }

    uint64_t pages = ((uint64_t)a->top + need + RC_PAGE - 1) / RC_PAGE;
    if (pages > RC_MAX_CHUNK_PAGES) {
        return false;
    }

    // Growing by at least the arena's chunk size keeps platform calls rare
    uint32_t extra = (uint32_t)pages - h->pages;
    uint32_t min_pages = rc_chunk(a->first)->min_pages;
    if (extra < min_pages && h->pages + min_pages <= RC_MAX_CHUNK_PAGES) {
        extra = min_pages;
    }

    if (!rc_pages_grow(extra)) {
        return false;
    }

    h->pages += extra;
    a->end = h->pages * (uint32_t)RC_PAGE;
    return true;
}


// The slow path of every allocation: make room for need bytes at top.
static void rc_arena_make_room(rc_arena *a, uint32_t need)
{
    rc_chunk_header *cur = rc_chunk(a->chunk);

    // A by-value copy of this arena may have grown our chunk since we cached its size
    a->end = cur->pages * (uint32_t)RC_PAGE;
    if (need <= a->end - a->top) {
        return;
    }

    // The next chunk in the chain is memory we already own, so it comes first
    if (cur->next && rc_chunk_fits(cur->next, need)) {
        rc_arena_enter_chunk(a, cur->next);
        return;
    }

    if (rc_arena_extend(a, need)) {
        return;
    }

    // Last resort: a fresh chunk, linked in after the current one
    uint32_t pages = rc_pages_for(need + RC_HEADER);
    uint32_t min_pages = rc_chunk(a->first)->min_pages;
    uint32_t page = rc_chunk_acquire(pages > min_pages ? pages : min_pages);
    if (!page) {
        rc_arena_out_of_memory(need);
    }

    rc_chunk(page)->next = cur->next;
    cur->next = page;
    rc_arena_enter_chunk(a, page);
}


// Offset of ptr within the current chunk, or RC_INDEX_NONE if it is outside the allocated part.
static uint32_t rc_arena_offset_of(const rc_arena *a, const void *ptr)
{
    uintptr_t base = (uintptr_t)rc_page_ptr(a->chunk);
    uintptr_t p = (uintptr_t)ptr;

    if (p < base + RC_HEADER || p - base > a->top) {
        return RC_INDEX_NONE;
    }
    return (uint32_t)(p - base);
}


static void *rc_arena_realloc_impl(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size)
{
    RC_ASSERT(new_size != 0);
    RC_ASSERT(old_size <= RC_MAX_ALLOC);

    if (new_size > RC_MAX_ALLOC) {
        rc_arena_out_of_memory(new_size);
    }

    uint32_t old_need = rc_align_up(old_size, RC_MAX_ALIGN);
    uint32_t new_need = rc_align_up(new_size, RC_MAX_ALIGN);
    uint32_t offset = rc_arena_offset_of(a, ptr);

    // The last allocation shrinks or grows in place, growing the chunk if need be
    if (offset != RC_INDEX_NONE && a->top - offset == old_need) {
        if (new_need <= old_need) {
            a->top = offset + new_need;
            return ptr;
        }

        uint32_t grow = new_need - old_need;
        a->end = rc_chunk_bytes(a->chunk);
        if (grow <= a->end - a->top || rc_arena_extend(a, grow)) {
            a->top += grow;
            return ptr;
        }
    }
    else if (new_need <= old_need) {
        return ptr;
    }

    void *result = rc_arena_alloc(a, new_size);
    rc_memcpy(result, ptr, old_size);
    return result;
}


// ---- public API ----

bool rc_arena_init(void)
{
    if (rc_mem.reserved_pages) {
        return true;
    }

    rc_heap heap = rc_heap_reserve();
    if (!heap.limit) {
        return false;
    }

    rc_mem = (rc_page_state) {
        .base = heap.base,
        .used_pages = heap.first,
        .reserved_pages = heap.limit,
        .pool = 0
    };
    return true;
}


void rc_arena_deinit(void)
{
    if (rc_mem.reserved_pages) {
        rc_heap_release(rc_mem.base);
    }
    rc_mem = (rc_page_state) {0};
}


rc_arena rc_arena_make(uint32_t chunk_size)
{
    if (!chunk_size) {
        chunk_size = RC_ARENA_DEFAULT_CHUNK;
    }

    // The page heap sets itself up on first use
    if (!rc_arena_init()) {
        rc_arena_out_of_memory(chunk_size);
    }

    uint32_t pages = rc_pages_for(chunk_size);
    RC_ASSERT(pages <= RC_MAX_CHUNK_PAGES);

    uint32_t first = rc_chunk_acquire(pages);
    if (!first) {
        rc_arena_out_of_memory(chunk_size);
    }

    rc_chunk(first)->min_pages = pages;
    return (rc_arena) {
        .first = first,
        .chunk = first,
        .top = RC_HEADER,
        .end = rc_chunk_bytes(first)
    };
}


void rc_arena_release(rc_arena *a)
{
    RC_ASSERT(a);

    uint32_t page = a->first;
    while (page) {
        uint32_t next = rc_chunk(page)->next;
        rc_pool_push(page);
        page = next;
    }
    *a = (rc_arena) {0};
}


void *rc_arena_alloc(rc_arena *a, uint32_t size)
{
    RC_ASSERT(a);
    RC_ASSERT(a->first);    // the arena must have been made
    RC_ASSERT(size != 0);

    if (size > RC_MAX_ALLOC) {
        rc_arena_out_of_memory(size);
    }

    uint32_t need = rc_align_up(size, RC_MAX_ALIGN);
    if (need > a->end - a->top) {
        rc_arena_make_room(a, need);
    }

    void *result = rc_page_ptr(a->chunk) + a->top;
    a->top += need;
    return result;
}


void *rc_arena_alloc_zero(rc_arena *a, uint32_t size)
{
    void *result = rc_arena_alloc(a, size);

    rc_memset(result, 0, size);
    return result;
}


void *rc_arena_alloc_aligned(rc_arena *a, uint32_t size, uint32_t align)
{
    RC_ASSERT(a);
    RC_ASSERT(a->first);
    RC_ASSERT(size != 0);
    RC_ASSERT(align != 0 && (align & (align - 1)) == 0);
    RC_ASSERT(align <= RC_ARENA_MAX_ALIGN);

    if (align <= RC_MAX_ALIGN) {
        return rc_arena_alloc(a, size);
    }

    // Chunks start page aligned, so in a fresh one the padding is at most align - RC_HEADER
    uint32_t worst_pad = align - RC_HEADER;
    if (size > RC_MAX_ALLOC - worst_pad) {
        rc_arena_out_of_memory(size);
    }

    uint32_t need = rc_align_up(size, RC_MAX_ALIGN);
    uint32_t pad = rc_align_up(a->top, align) - a->top;
    if (pad > a->end - a->top || need > a->end - a->top - pad) {
        rc_arena_make_room(a, need + worst_pad);
        pad = rc_align_up(a->top, align) - a->top;
    }

    void *result = rc_page_ptr(a->chunk) + a->top + pad;
    a->top += pad + need;
    return result;
}


bool rc_arena_free(rc_arena *a, void *ptr, uint32_t size)
{
    RC_ASSERT(a);
    RC_ASSERT(a->first);
    RC_ASSERT(ptr);

    if (size > RC_MAX_ALLOC) {
        return false;
    }

    uint32_t offset = rc_arena_offset_of(a, ptr);
    if (offset == RC_INDEX_NONE || a->top - offset != rc_align_up(size, RC_MAX_ALIGN)) {
        return false;
    }

    a->top = offset;
    return true;
}


void *rc_arena_realloc(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size)
{
    RC_ASSERT(a);
    RC_ASSERT(a->first);

    if (!ptr) {
        return rc_arena_alloc(a, new_size);
    }
    return rc_arena_realloc_impl(a, ptr, old_size, new_size);
}


void *rc_arena_realloc_zero(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size)
{
    RC_ASSERT(a);
    RC_ASSERT(a->first);

    if (!ptr) {
        return rc_arena_alloc_zero(a, new_size);
    }

    uint8_t *result = rc_arena_realloc_impl(a, ptr, old_size, new_size);
    if (new_size > old_size) {
        rc_memset(result + old_size, 0, new_size - old_size);
    }
    return result;
}


void rc_arena_reset(rc_arena *a)
{
    RC_ASSERT(a);

    if (a->first) {
        rc_arena_enter_chunk(a, a->first);
    }
}


rc_arena_mark rc_arena_save(const rc_arena *a)
{
    RC_ASSERT(a);

    return (rc_arena_mark) {
        .chunk = a->chunk,
        .top = a->top
    };
}


void rc_arena_restore(rc_arena *a, rc_arena_mark mark)
{
    RC_ASSERT(a);
    RC_ASSERT(mark.chunk != 0);

    rc_arena_enter_chunk(a, mark.chunk);
    RC_ASSERT(mark.top >= RC_HEADER && mark.top <= a->end);
    a->top = mark.top;
}


void rc_arena_set_oom_handler(rc_arena_oom_fn *fn)
{
    rc_oom_handler = fn;
}
