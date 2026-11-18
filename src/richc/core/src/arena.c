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

// ---- pages and chunks ----

// The address of a page.
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


// The size of a chunk in bytes, header included.
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


// Give the handler its say about a failed request of size bytes, then stop.
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
    uint32_t page = rc_mem.used_pages;

    if (count > rc_mem.reserved_pages - page) {
        return 0;
    }

    if (!rc_heap_commit(rc_mem.base, page, count)) {
        return 0;
    }

    rc_mem.used_pages += count;
    return page;
}


// Write a fresh header, linked to nothing, at the start of a chunk of pages pages.
static void rc_chunk_init(uint32_t page, uint32_t pages)
{
    *(rc_chunk_header *)rc_page_ptr(page) = (rc_chunk_header) {
        .sentinel = RC_CHUNK_SENTINEL ^ page,
        .next = 0,
        .pages = pages,
        .min_pages = 0
    };
}


// True if need bytes fit in the chunk after its header.
static bool rc_chunk_fits(uint32_t page, uint32_t need)
{
    return need <= rc_chunk_bytes(page) - RC_HEADER;
}

// ---- the pool ----

// Put one chunk at the front of the pool.
static void rc_pool_push(uint32_t page)
{
    rc_chunk(page)->next = rc_mem.pool;
    rc_mem.pool = page;
}


// Put a whole chain of chunks, from page on, into the pool.
static void rc_pool_push_chain(uint32_t page)
{
    while (page) {
        uint32_t next = rc_chunk(page)->next;

        rc_pool_push(page);
        page = next;
    }
}


// True if a chunk of have pages holds pages, and fits them better than the one at best (if any).
static bool rc_pool_is_better_fit(uint32_t have, uint32_t pages, const uint32_t *best)
{
    if (have < pages) {
        return false;
    }

    return !best || have < rc_chunk(*best)->pages;
}


// The link to the smallest pooled chunk of at least pages pages, stopping early at an exact match;
// NULL if none is big enough.
static uint32_t *rc_pool_best_fit(uint32_t pages)
{
    uint32_t *best = NULL;

    for (uint32_t *link = &rc_mem.pool; *link; link = &rc_chunk(*link)->next) {
        uint32_t have = rc_chunk(*link)->pages;

        if (rc_pool_is_better_fit(have, pages, best)) {
            best = link;
        }

        if (have == pages) {
            break;
        }
    }

    return best;
}


// Unlink the best fit for pages from the pool; 0 if none is big enough.
static uint32_t rc_pool_take(uint32_t pages)
{
    uint32_t *best = rc_pool_best_fit(pages);
    uint32_t page = best ? *best : 0;

    if (!page) {
        return 0;
    }

    *best = rc_chunk(page)->next;
    rc_chunk(page)->next = 0;
    return page;
}


// A new chunk of pages pages at the end of the heap; 0 when out of memory.
static uint32_t rc_chunk_fresh(uint32_t pages)
{
    uint32_t page = rc_pages_grow(pages);

    if (page) {
        rc_chunk_init(page, pages);
    }

    return page;
}


// A chunk of at least pages pages: the best fit from the pool, else fresh from the heap. 0 when out
// of memory.
static uint32_t rc_chunk_acquire(uint32_t pages)
{
    uint32_t page = rc_pool_take(pages);

    return page ? page : rc_chunk_fresh(pages);
}

// ---- making room ----

// Make page the arena's current chunk, empty.
static void rc_arena_enter_chunk(rc_arena *a, uint32_t page)
{
    a->chunk = page;
    a->top = RC_HEADER;
    a->end = rc_chunk_bytes(page);
}


// The pages to add to a chunk of have pages that needs extra more. Growing by at least the arena's
// chunk size, while that stays within a maximal chunk, keeps platform calls rare.
static uint32_t rc_chunk_growth(uint32_t have, uint32_t extra, uint32_t min_pages)
{
    if (extra >= min_pages) {
        return extra;
    }

    if (have + min_pages > RC_MAX_CHUNK_PAGES) {
        return extra;
    }

    return min_pages;
}


// Commit extra pages straight after the current chunk and take them into it.
static bool rc_arena_extend_by(rc_arena *a, uint32_t extra)
{
    rc_chunk_header *h = rc_chunk(a->chunk);

    if (!rc_pages_grow(extra)) {
        return false;
    }

    h->pages += extra;
    a->end = h->pages * (uint32_t)RC_PAGE;
    return true;
}


// Grow the current chunk in place so need more bytes fit after top. Only the newest chunk in the
// heap can grow, since the pages after it are free.
static bool rc_arena_extend(rc_arena *a, uint32_t need)
{
    rc_chunk_header *h = rc_chunk(a->chunk);
    uint64_t pages = ((uint64_t)a->top + need + RC_PAGE - 1) / RC_PAGE;

    if (a->chunk + h->pages != rc_mem.used_pages) {
        return false;
    }

    if (pages > RC_MAX_CHUNK_PAGES) {
        return false;
    }

    return rc_arena_extend_by(a, rc_chunk_growth(h->pages, (uint32_t)pages - h->pages, rc_chunk(a->first)->min_pages));
}


// Link a fresh chunk with room for need bytes in after the current one, and move into it.
static void rc_arena_add_chunk(rc_arena *a, uint32_t need)
{
    uint32_t pages = rc_pages_for(need + RC_HEADER);
    uint32_t min_pages = rc_chunk(a->first)->min_pages;
    uint32_t page = rc_chunk_acquire(pages > min_pages ? pages : min_pages);

    if (!page) {
        rc_arena_out_of_memory(need);
    }

    rc_chunk(page)->next = rc_chunk(a->chunk)->next;
    rc_chunk(a->chunk)->next = page;
    rc_arena_enter_chunk(a, page);
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

    rc_arena_add_chunk(a, need);
}


// True if pad bytes and then need bytes fit after top in the current chunk.
static bool rc_arena_fits_padded(const rc_arena *a, uint32_t pad, uint32_t need)
{
    uint32_t room = a->end - a->top;

    if (pad > room) {
        return false;
    }

    return need <= room - pad;
}

// ---- allocating ----

// Skip pad bytes, then hand out the next need bytes, which the current chunk has room for.
static void *rc_arena_take(rc_arena *a, uint32_t pad, uint32_t need)
{
    uint8_t *result = rc_page_ptr(a->chunk) + a->top + pad;

    a->top += pad + need;
    return result;
}


// Hand out need (aligned) bytes, making room for them first if need be.
static void *rc_arena_bump(rc_arena *a, uint32_t need)
{
    if (need > a->end - a->top) {
        rc_arena_make_room(a, need);
    }

    return rc_arena_take(a, 0, need);
}


// Hand out need (aligned) bytes at a multiple of align, a power of two above RC_MAX_ALIGN.
static void *rc_arena_bump_aligned(rc_arena *a, uint32_t need, uint32_t align)
{
    uint32_t pad = rc_align_up(a->top, align) - a->top;

    // Chunks start page aligned, so in a fresh one the padding is at most align - RC_HEADER
    if (!rc_arena_fits_padded(a, pad, need)) {
        rc_arena_make_room(a, need + align - RC_HEADER);
        pad = rc_align_up(a->top, align) - a->top;
    }

    return rc_arena_take(a, pad, need);
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


// True if the allocation at offset, need (aligned) bytes long, is the last in the current chunk.
static bool rc_arena_is_last(const rc_arena *a, uint32_t offset, uint32_t need)
{
    return offset != RC_INDEX_NONE && a->top - offset == need;
}


// Rewind over ptr, need (aligned) bytes long, if it is the last allocation.
static bool rc_arena_free_last(rc_arena *a, const void *ptr, uint32_t need)
{
    uint32_t offset = rc_arena_offset_of(a, ptr);

    if (!rc_arena_is_last(a, offset, need)) {
        return false;
    }

    a->top = offset;
    return true;
}

// ---- resizing ----

// True if need more bytes fit after top, growing the chunk in place if they don't yet.
static bool rc_arena_room_in_place(rc_arena *a, uint32_t need)
{
    // A by-value copy of this arena may have grown our chunk since we cached its size
    a->end = rc_chunk_bytes(a->chunk);
    if (need <= a->end - a->top) {
        return true;
    }

    return rc_arena_extend(a, need);
}


// Resize the last allocation, at offset, in place, growing the chunk if need be. False if there is
// no room to grow.
static bool rc_arena_resize_last(rc_arena *a, uint32_t offset, uint32_t old_need, uint32_t new_need)
{
    uint32_t grow = new_need > old_need ? new_need - old_need : 0;

    if (grow == 0) {
        a->top = offset + new_need;
        return true;
    }

    if (!rc_arena_room_in_place(a, grow)) {
        return false;
    }

    a->top += grow;
    return true;
}


// A new allocation of new_size bytes, starting with the old_size bytes at ptr.
static void *rc_arena_move(rc_arena *a, const void *ptr, uint32_t old_size, uint32_t new_size)
{
    void *result = rc_arena_alloc(a, new_size);

    rc_memcpy(result, ptr, old_size);
    return result;
}


// Resize ptr, both sizes within the limit: in place when it is the last allocation or shrinking,
// else by moving it.
static void *rc_arena_resize(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size)
{
    uint32_t old_need = rc_align_up(old_size, RC_MAX_ALIGN);
    uint32_t new_need = rc_align_up(new_size, RC_MAX_ALIGN);
    uint32_t offset = rc_arena_offset_of(a, ptr);

    if (rc_arena_is_last(a, offset, old_need)) {
        if (rc_arena_resize_last(a, offset, old_need, new_need)) {
            return ptr;
        }
    }
    else if (new_need <= old_need) {
        return ptr;
    }

    return rc_arena_move(a, ptr, old_size, new_size);
}


// Resize a non-NULL ptr, panicking if the new size is too large.
static void *rc_arena_realloc_impl(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size)
{
    RC_ASSERT(new_size != 0);
    RC_ASSERT(old_size <= RC_MAX_ALLOC);

    if (new_size > RC_MAX_ALLOC) {
        rc_arena_out_of_memory(new_size);
    }

    return rc_arena_resize(a, ptr, old_size, new_size);
}

// ---- setting up ----

// Take on a fresh reservation as the page heap; false if the reserve failed.
static bool rc_arena_adopt_heap(rc_heap heap)
{
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


// An arena on a first chunk of pages pages, which also sets the size it grows by.
static rc_arena rc_arena_make_pages(uint32_t pages, uint32_t chunk_size)
{
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

// ---- public API ----

bool rc_arena_init(void)
{
    if (rc_mem.reserved_pages) {
        return true;
    }

    return rc_arena_adopt_heap(rc_heap_reserve());
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
    uint32_t size = chunk_size ? chunk_size : RC_ARENA_DEFAULT_CHUNK;
    uint32_t pages = rc_pages_for(size);

    // The page heap sets itself up on first use
    if (!rc_arena_init()) {
        rc_arena_out_of_memory(size);
    }

    RC_ASSERT(pages <= RC_MAX_CHUNK_PAGES);
    return rc_arena_make_pages(pages, size);
}


void rc_arena_release(rc_arena *a)
{
    RC_ASSERT(a);

    rc_pool_push_chain(a->first);
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

    return rc_arena_bump(a, rc_align_up(size, RC_MAX_ALIGN));
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

    // The worst padding a fresh chunk needs, align - RC_HEADER, must fit too
    if (size > RC_MAX_ALLOC - (align - RC_HEADER)) {
        rc_arena_out_of_memory(size);
    }

    return rc_arena_bump_aligned(a, rc_align_up(size, RC_MAX_ALIGN), align);
}


bool rc_arena_free(rc_arena *a, void *ptr, uint32_t size)
{
    RC_ASSERT(a);
    RC_ASSERT(a->first);
    RC_ASSERT(ptr);

    if (size > RC_MAX_ALLOC) {
        return false;
    }

    return rc_arena_free_last(a, ptr, rc_align_up(size, RC_MAX_ALIGN));
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
    uint32_t kept = ptr ? old_size : 0;
    uint8_t *result = rc_arena_realloc(a, ptr, old_size, new_size);

    // Everything past what came over from ptr is new, and zeroed
    if (new_size > kept) {
        rc_memset(result + kept, 0, new_size - kept);
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
