// arena.c
//
// Chunked arenas over the global page heap, plus the pool of free pages that release gives back.

#include "richc/arena.h"

#include <stddef.h>

#include "heap.h"
#include "richc/mem.h"

enum {
    RC_PAGE = RC_HEAP_PAGE_SIZE,
    RC_HEADER = RC_MAX_ALIGN,           // chunk header size, so allocations start aligned
    RC_MAX_CHUNK_PAGES = 0xFFFF,        // keeps a chunk's size in bytes within uint32_t
    RC_RETAIN_PAGES = 256               // free pages kept committed for reuse: 16 MiB
};

// Largest single allocation: a maximal chunk, less its header.
#define RC_MAX_ALLOC ((uint32_t)RC_MAX_CHUNK_PAGES * RC_PAGE - RC_HEADER)

// XORed with the header's own page index, so a stray or misplaced header fails too.
#define RC_CHUNK_SENTINEL 0x52434348u
#define RC_FREE_SENTINEL 0x52434652u

// Lives at the start of every chunk. Chunks sit end to end in the heap, so an overrun off the end
// of one lands on the next one's sentinel. Every page of a chunk is committed.
typedef struct rc_chunk_header {
    uint32_t sentinel;
    uint32_t next;       // next chunk in the arena's chain
    uint32_t pages;
    uint32_t min_pages;  // growth size, read from an arena's first chunk only
} rc_chunk_header;

// Lives at the start of every free run of pages in the pool, where a chunk's header would be.
typedef struct rc_free_header {
    uint32_t sentinel;
    uint32_t next;       // next free run up in the pool
    uint32_t pages;
    uint32_t committed;  // how many pages from the first are committed (at least that one); the rest
                         // have gone back to the OS
} rc_free_header;

RC_STATIC_ASSERT(sizeof(rc_chunk_header) == RC_HEADER);
RC_STATIC_ASSERT(sizeof(rc_free_header) == RC_HEADER);
RC_STATIC_ASSERT(RC_ARENA_MAX_ALIGN == RC_PAGE);
#if !defined(_MSC_VER) || defined(__clang__)
RC_STATIC_ASSERT(alignof(max_align_t) <= RC_MAX_ALIGN);
#endif

// The global page heap. Pages from the first one up to used_pages each belong to a chunk in an
// arena, or to a free run in the pool. Free pages, in the pool or above the top, stay committed for
// the next chunk to take without a platform call, until there are more than RC_RETAIN_PAGES of
// them; past that they go back to the OS. On wasm, which cannot take pages back, they stay as they
// were.
typedef struct rc_page_state {
    uintptr_t base;            // address of page 0
    uint32_t used_pages;       // the top: one past the highest page in use or in the pool
    uint32_t reserved_pages;   // one past the last page we may use; 0 until init
    uint32_t pool;             // the lowest free run, linked to the rest in address order
    uint32_t spare_pages;      // how many pages from the top up are committed; the rest are not
    uint32_t retained_pages;   // free pages committed: the spare ones, and those of the free runs
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


// Give count pages from page on back to the OS, if there are any.
static void rc_pages_decommit(uint32_t page, uint32_t count)
{
    if (count != 0) {
        rc_heap_decommit(rc_mem.base, page, count);
    }
}


// Take count more pages at the top of the heap, committing those past the spare ones. Returns the
// first, or 0 when out of memory.
static uint32_t rc_pages_grow(uint32_t count)
{
    uint32_t page = rc_mem.used_pages;
    uint32_t spare = rc_mem.spare_pages < count ? rc_mem.spare_pages : count;

    if (count > rc_mem.reserved_pages - page) {
        return 0;
    }

    if (spare < count && !rc_heap_commit(rc_mem.base, page + spare, count - spare)) {
        return 0;
    }

    rc_mem.used_pages += count;
    rc_mem.spare_pages -= spare;
    rc_mem.retained_pages -= spare;
    return page;
}


// Give back to the OS up to excess of the spare pages at the top, the highest first.
static void rc_top_trim(uint32_t excess)
{
    uint32_t trimmed = rc_mem.spare_pages < excess ? rc_mem.spare_pages : excess;

    rc_mem.spare_pages -= trimmed;
    rc_mem.retained_pages -= trimmed;
    rc_pages_decommit(rc_mem.used_pages + rc_mem.spare_pages, trimmed);
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
//
// The pool holds the runs of pages that arenas have released, in address order, linked through
// their headers. A run released next to a free one merges with it, so no two free runs are ever
// side by side, and one that reaches the top of the heap is given back to it, so none ever ends
// there and the chunk below can grow in place again. Pages are cut from the front of a run, which
// leaves the rest straight after them, for the new chunk to grow into.
//
// The committed pages of a run are always its first ones, at least the one that holds its header,
// and those cut from it that are not are committed as they are.

// The header of the free run at page, checked by its sentinel.
static rc_free_header *rc_free_run(uint32_t page)
{
    rc_free_header *h = (rc_free_header *)rc_page_ptr(page);

    RC_ASSERT(page != 0);
    RC_ASSERT(h->sentinel == (RC_FREE_SENTINEL ^ page));   // something wrote to released memory
    return h;
}


// Write the header of a free run of pages pages at page, the first committed of them committed,
// followed in the pool by next.
static void rc_free_run_init(uint32_t page, uint32_t pages, uint32_t committed, uint32_t next)
{
    *(rc_free_header *)rc_page_ptr(page) = (rc_free_header) {
        .sentinel = RC_FREE_SENTINEL ^ page,
        .next = next,
        .pages = pages,
        .committed = committed
    };
}


// One past the last page of the free run at page.
static uint32_t rc_free_run_end(uint32_t page)
{
    return page + rc_free_run(page)->pages;
}


// True if the free run at lower ends where the one at upper starts. Either may be 0, for none.
static bool rc_free_runs_meet(uint32_t lower, uint32_t upper)
{
    if (lower == 0 || upper == 0) {
        return false;
    }
    return rc_free_run_end(lower) == upper;
}


// Merge the free run at upper into the one at lower, which it meets and follows in the pool.
// upper's committed pages carry on from lower's if all of those are committed; if not, they would
// sit past pages that are not, so they go back to the OS. The header upper leaves behind is wiped
// first, so a stale pointer to the chunk that was there fails its sentinel check.
static void rc_free_run_absorb(uint32_t lower, uint32_t upper)
{
    rc_free_header *low = rc_free_run(lower);
    rc_free_header *up_header = rc_free_run(upper);
    rc_free_header up = *up_header;

    *up_header = (rc_free_header) {0};
    if (low->committed == low->pages) {
        low->committed += up.committed;
    }
    else {
        rc_pages_decommit(upper, up.committed);
        rc_mem.retained_pages -= up.committed;
    }

    low->pages += up.pages;
    low->next = up.next;
}


// Give back to the OS up to excess of the committed pages of the free run at page, the last first,
// keeping the one that holds its header.
static void rc_free_run_trim(uint32_t page, uint32_t excess)
{
    rc_free_header *run = rc_free_run(page);
    uint32_t spare = run->committed - 1;
    uint32_t trimmed = spare < excess ? spare : excess;

    run->committed -= trimmed;
    rc_mem.retained_pages -= trimmed;
    rc_pages_decommit(page + run->committed, trimmed);
}


// Commit the first pages pages of the free run at page. False when out of memory.
static bool rc_free_run_commit(uint32_t page, uint32_t pages)
{
    uint32_t committed = rc_free_run(page)->committed;

    if (pages <= committed) {
        return true;
    }
    return rc_heap_commit(rc_mem.base, page + committed, pages - committed);
}


// The last free run below page, or 0 if there is none.
static uint32_t rc_pool_below(uint32_t page)
{
    uint32_t below = 0;
    uint32_t run = rc_mem.pool;

    while (run != 0 && run < page) {
        below = run;
        run = rc_free_run(run)->next;
    }
    return below;
}


// The link to the first free run at or above page, which is where a run at page belongs.
static uint32_t *rc_pool_link_at(uint32_t page)
{
    uint32_t *link = &rc_mem.pool;

    while (*link != 0 && *link < page) {
        link = &rc_free_run(*link)->next;
    }
    return link;
}


// Add the pages pages of a chunk at page to the pool, merged with the free runs either side if they
// meet.
static void rc_pool_add(uint32_t page, uint32_t pages)
{
    uint32_t below = rc_pool_below(page);
    uint32_t *link = below != 0 ? &rc_free_run(below)->next : &rc_mem.pool;
    uint32_t above = *link;

    rc_free_run_init(page, pages, pages, above);
    rc_mem.retained_pages += pages;
    *link = page;
    if (rc_free_runs_meet(page, above)) {
        rc_free_run_absorb(page, above);
    }

    if (rc_free_runs_meet(below, page)) {
        rc_free_run_absorb(below, page);
    }
}


// Lower the top of the heap to the free run at page, which reaches it. The run's committed pages
// become spare, and so do the spare ones above it, if they carry on from them; if not, they go back
// to the OS.
static void rc_top_lower_to(uint32_t page)
{
    rc_free_header *run = rc_free_run(page);
    uint32_t spare = run->committed;

    if (run->committed == run->pages) {
        spare += rc_mem.spare_pages;
    }
    else {
        rc_top_trim(rc_mem.spare_pages);
    }

    rc_mem.used_pages = page;
    rc_mem.spare_pages = spare;
}


// Give the highest free run back to the top of the heap, if it reaches the top. Free runs never
// meet, so no other can reach the new top.
static void rc_pool_give_back_top(void)
{
    uint32_t *link = &rc_mem.pool;

    // The highest run is the last
    while (*link != 0 && rc_free_run(*link)->next != 0) {
        link = &rc_free_run(*link)->next;
    }

    if (*link != 0 && rc_free_run_end(*link) == rc_mem.used_pages) {
        rc_top_lower_to(*link);
        *link = 0;
    }
}


// How many more free pages are committed than are kept for reuse.
static uint32_t rc_retained_excess(void)
{
    return rc_mem.retained_pages > RC_RETAIN_PAGES ? rc_mem.retained_pages - RC_RETAIN_PAGES : 0;
}


// Give pages back to the OS until the free pages committed are within RC_RETAIN_PAGES: the spare
// ones at the top first, then those of each free run in turn, from the lowest.
static void rc_pool_trim(void)
{
    rc_top_trim(rc_retained_excess());
    for (uint32_t run = rc_mem.pool; run != 0 && rc_retained_excess() != 0; run = rc_free_run(run)->next) {
        rc_free_run_trim(run, rc_retained_excess());
    }
}


// Release a whole chain of chunks, from page on, to the pool.
static void rc_pool_release_chain(uint32_t page)
{
    while (page != 0) {
        uint32_t next = rc_chunk(page)->next;

        rc_pool_add(page, rc_chunk(page)->pages);
        page = next;
    }

    rc_pool_give_back_top();
    rc_pool_trim();
}


// True if a free run of have pages holds pages, and fits them better than the one at best (if any).
static bool rc_pool_is_better_fit(uint32_t have, uint32_t pages, const uint32_t *best)
{
    if (have < pages) {
        return false;
    }

    return !best || have < rc_free_run(*best)->pages;
}


// The link to the smallest free run of at least pages pages, stopping early at an exact match;
// NULL if none is big enough.
static uint32_t *rc_pool_best_fit(uint32_t pages)
{
    uint32_t *best = NULL;

    for (uint32_t *link = &rc_mem.pool; *link; link = &rc_free_run(*link)->next) {
        uint32_t have = rc_free_run(*link)->pages;

        if (rc_pool_is_better_fit(have, pages, best)) {
            best = link;
        }

        if (have == pages) {
            break;
        }
    }

    return best;
}


// Cut pages pages from the front of the free run that link points at, committed, leaving the rest
// of it, if any, as a free run in its place. Returns the first page cut; 0 when out of memory, with
// nothing cut.
static uint32_t rc_pool_cut(uint32_t *link, uint32_t pages)
{
    uint32_t page = *link;
    rc_free_header run = *rc_free_run(page);
    uint32_t rest = run.pages - pages;

    // The rest needs its first page committed too, for its header
    uint32_t commit = rest != 0 ? pages + 1 : pages;
    uint32_t committed = run.committed > commit ? run.committed : commit;
    uint32_t rest_committed = rest != 0 ? committed - pages : 0;

    if (!rc_free_run_commit(page, commit)) {
        return 0;
    }

    rc_mem.retained_pages = rc_mem.retained_pages - run.committed + rest_committed;
    if (rest == 0) {
        *link = run.next;
        return page;
    }

    rc_free_run_init(page + pages, rest, rest_committed, run.next);
    *link = page + pages;
    return page;
}


// pages pages cut from the best-fitting free run, returning the first; 0 if none is big enough, or
// when out of memory.
static uint32_t rc_pool_take(uint32_t pages)
{
    uint32_t *best = rc_pool_best_fit(pages);

    if (!best) {
        return 0;
    }
    return rc_pool_cut(best, pages);
}


// Pages cut from the front of the free run starting at page, if there is one of at least needed:
// wanted of them, or all of it if it has fewer. Returns how many; 0 if there is no such run, or
// when out of memory.
static uint32_t rc_pool_take_at(uint32_t page, uint32_t needed, uint32_t wanted)
{
    uint32_t *link = rc_pool_link_at(page);
    uint32_t have = *link == page ? rc_free_run(page)->pages : 0;
    uint32_t taken = have < wanted ? have : wanted;

    if (have < needed) {
        return 0;
    }

    return rc_pool_cut(link, taken) != 0 ? taken : 0;
}


// A new chunk of pages pages at the top of the heap; 0 when out of memory.
static uint32_t rc_chunk_fresh(uint32_t pages)
{
    uint32_t page = rc_pages_grow(pages);

    if (page) {
        rc_chunk_init(page, pages);
    }

    return page;
}


// A chunk of pages pages: cut from the best-fitting free run, else fresh from the top of the heap.
// 0 when out of memory.
static uint32_t rc_chunk_acquire(uint32_t pages)
{
    uint32_t page = rc_pool_take(pages);

    if (page == 0) {
        return rc_chunk_fresh(pages);
    }

    rc_chunk_init(page, pages);
    return page;
}


// A chunk of wanted pages, else of needed, the fewest that will do. 0 when out of memory.
static uint32_t rc_chunk_acquire_at_least(uint32_t needed, uint32_t wanted)
{
    uint32_t page = rc_chunk_acquire(wanted);

    if (page != 0 || wanted == needed) {
        return page;
    }
    return rc_chunk_acquire(needed);
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


// Pages committed at the top of the heap: wanted of them, or failing that needed. Returns how many;
// 0 when out of memory.
static uint32_t rc_pages_at_top(uint32_t needed, uint32_t wanted)
{
    if (rc_pages_grow(wanted)) {
        return wanted;
    }
    return rc_pages_grow(needed) ? needed : 0;
}


// Pages to grow a chunk ending at end by, wanted of them if there is room, else fewer but never
// under needed: committed at the top of the heap if the chunk ends there, or else cut from the free
// run starting at end. Returns how many; 0 if there is no room after the chunk.
static uint32_t rc_pages_after(uint32_t end, uint32_t needed, uint32_t wanted)
{
    if (end == rc_mem.used_pages) {
        return rc_pages_at_top(needed, wanted);
    }
    return rc_pool_take_at(end, needed, wanted);
}


// Grow the current chunk in place to at least pages pages, from the pages straight after it.
// False if they are in use.
static bool rc_arena_grow_chunk(rc_arena *a, uint32_t pages)
{
    rc_chunk_header *h = rc_chunk(a->chunk);
    uint32_t needed = pages - h->pages;
    uint32_t wanted = rc_chunk_growth(h->pages, needed, rc_chunk(a->first)->min_pages);
    uint32_t taken = rc_pages_after(a->chunk + h->pages, needed, wanted);

    if (taken == 0) {
        return false;
    }

    h->pages += taken;
    a->end = h->pages * (uint32_t)RC_PAGE;
    return true;
}


// Grow the current chunk in place so need more bytes fit after top: into the top of the heap, or
// into a free run that follows it.
static bool rc_arena_extend(rc_arena *a, uint32_t need)
{
    uint64_t pages = ((uint64_t)a->top + need + RC_PAGE - 1) / RC_PAGE;

    if (pages > RC_MAX_CHUNK_PAGES) {
        return false;
    }
    return rc_arena_grow_chunk(a, (uint32_t)pages);
}


// Link a fresh chunk with room for need bytes in after the current one, and move into it.
static void rc_arena_add_chunk(rc_arena *a, uint32_t need)
{
    uint32_t pages = rc_pages_for(need + RC_HEADER);
    uint32_t min_pages = rc_chunk(a->first)->min_pages;
    uint32_t page = rc_chunk_acquire_at_least(pages, pages > min_pages ? pages : min_pages);

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
        .pool = 0,
        .spare_pages = 0,
        .retained_pages = 0
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

    // Within a few KiB of 4 GiB, the largest chunk there can be is the closest
    return rc_arena_make_pages(pages < RC_MAX_CHUNK_PAGES ? pages : RC_MAX_CHUNK_PAGES, size);
}


void rc_arena_release(rc_arena *a)
{
    RC_ASSERT(a);

    rc_pool_release_chain(a->first);
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
