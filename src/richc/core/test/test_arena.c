// test_arena.c
//
// Tests for the chunked arena.
//
// The runner gives each test a fresh page heap, so with the pool empty every chunk an arena makes
// is new at the end of the heap. A "blocker" arena made after another stops that one's chunk from
// growing in place.

#include "richc/arena.h"

#include <setjmp.h>

#include "richc/mem.h"
#include "richc/random.h"
#include "richc/test.h"

enum {
    PAGE = 65536
};

// True if p is a multiple of align.
static bool is_aligned(const void *p, uint32_t align)
{
    return (uintptr_t)p % align == 0;
}


// Fill size bytes at p with a pattern derived from seed.
static void fill(uint8_t *p, uint32_t size, uint8_t seed)
{
    for (uint32_t i = 0; i < size; i++) {
        p[i] = (uint8_t)(seed + i);
    }
}


// True if the size bytes at p still hold seed's pattern.
static bool check_fill(const uint8_t *p, uint32_t size, uint8_t seed)
{
    for (uint32_t i = 0; i < size; i++) {
        if (p[i] != (uint8_t)(seed + i)) {
            return false;
        }
    }
    return true;
}


// size bytes from arena, filled with seed's pattern.
static uint8_t *alloc_filled(rc_arena *arena, uint32_t size, uint8_t seed)
{
    uint8_t *p = rc_arena_alloc(arena, size);

    fill(p, size, seed);
    return p;
}


// size bytes filled with seed's pattern, with another block after them so they are not the last.
static uint8_t *alloc_buried(rc_arena *arena, uint32_t size, uint8_t seed)
{
    uint8_t *p = alloc_filled(arena, size, seed);

    rc_arena_alloc(arena, 16);
    return p;
}


// Dirty size bytes at the top of arena and free them again, so the next allocation lands on them.
static uint8_t *alloc_dirty_and_free(rc_arena *arena, uint32_t size, uint8_t seed)
{
    uint8_t *p = alloc_filled(arena, size, seed);

    RC_CHECK_TRUE(rc_arena_free(arena, p, size));
    return p;
}


// size bytes aligned to align, allocated after a plain block of first_size bytes.
static uint8_t *alloc_aligned_after(rc_arena *arena, uint32_t first_size, uint32_t size, uint32_t align)
{
    rc_arena_alloc(arena, first_size);
    return rc_arena_alloc_aligned(arena, size, align);
}


// Release a, then make an arena of chunk_size, which may take a's chunks from the pool.
static rc_arena release_and_make(rc_arena *a, uint32_t chunk_size)
{
    rc_arena_release(a);
    return rc_arena_make(chunk_size);
}


// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(arena) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(arena, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(arena, fix)
{
    rc_arena_release(&fix->a);
}


// ---- lifecycle ----

RC_TEST(arena, make_default)
{
    rc_arena a = rc_arena_make_default();

    RC_CHECK(a.first, !=, 0u);
    RC_CHECK(a.chunk, ==, a.first);
    RC_CHECK(a.top, ==, RC_MAX_ALIGN);
    RC_CHECK(a.end, >=, (uint32_t)PAGE);
    RC_CHECK(a.end % PAGE, ==, 0u);
    rc_arena_release(&a);
}


RC_TEST(arena, make_zero_is_default)
{
    rc_arena a = rc_arena_make(0);

    RC_CHECK(a.first, !=, 0u);
    RC_CHECK(a.end, >=, RC_ARENA_DEFAULT_CHUNK);
    rc_arena_release(&a);
}


RC_TEST(arena, make_rounds_up_to_pages)
{
    rc_arena small = rc_arena_make(100);
    rc_arena bigger = rc_arena_make(PAGE + 1);

    RC_CHECK(small.end, >=, (uint32_t)PAGE);
    RC_CHECK(bigger.end, >=, 2u * PAGE);
    RC_CHECK(bigger.end % PAGE, ==, 0u);
    rc_arena_release(&small);
    rc_arena_release(&bigger);
}


RC_TEST(arena, release_zeroes)
{
    rc_arena a = rc_arena_make_default();

    rc_arena_release(&a);
    RC_CHECK(a.first, ==, 0u);
    RC_CHECK(a.chunk, ==, 0u);
    RC_CHECK(a.top, ==, 0u);
    RC_CHECK(a.end, ==, 0u);
}


RC_TEST(arena, release_zero_arena_is_noop)
{
    rc_arena a = {0};

    rc_arena_release(&a);
    RC_CHECK(a.first, ==, 0u);
}


RC_TEST(arena, release_recycles_chunk)
{
    // An odd size, so the best fit from the pool is exactly our chunk
    rc_arena a = rc_arena_make(7 * PAGE);
    uint32_t first = a.first;
    rc_arena b = release_and_make(&a, 7 * PAGE);

    RC_CHECK(b.first, ==, first);
    rc_arena_release(&b);
}


// Fill a's first chunk, of chunk_size, and spill into a second, returning the second's page.
static uint32_t spill_into_second_chunk(rc_arena *a, uint32_t chunk_size)
{
    rc_arena_alloc(a, chunk_size - RC_MAX_ALIGN);
    rc_arena_alloc(a, 1000);
    return a->chunk;
}


RC_TEST(arena, release_recycles_whole_chain)
{
    rc_arena a = rc_arena_make(8 * PAGE);
    rc_arena blocker = rc_arena_make(PAGE);
    uint32_t second = spill_into_second_chunk(&a, 8 * PAGE);
    uint32_t first = a.first;
    rc_arena b = release_and_make(&a, 8 * PAGE);
    rc_arena c = rc_arena_make(8 * PAGE);

    RC_CHECK(second, !=, first);

    // a's first chunk, below the blocker, went to the pool; its second, at the top of the heap,
    // went back to the heap, which hands the same pages out again
    RC_CHECK(b.first, ==, first);
    RC_CHECK(c.first, ==, second);
    rc_arena_release(&b);
    rc_arena_release(&c);
    rc_arena_release(&blocker);
}


RC_TEST(arena, is_same)
{
    rc_arena a = rc_arena_make_default();
    rc_arena b = rc_arena_make_default();
    rc_arena copy = a;
    rc_arena zero = {0};

    RC_CHECK_TRUE(rc_arena_is_same(&a, &copy));
    RC_CHECK_TRUE(rc_arena_is_same(&copy, &a));
    RC_CHECK_FALSE(rc_arena_is_same(&a, &b));
    RC_CHECK_FALSE(rc_arena_is_same(&zero, &zero));
    RC_CHECK_FALSE(rc_arena_is_same(&a, &zero));
    rc_arena_release(&a);
    rc_arena_release(&b);
}


// ---- page heap ----

RC_TEST(arena, each_test_starts_with_empty_heap)
{
    rc_arena a = rc_arena_make(PAGE);

    // The first page the heap hands out
    RC_CHECK(a.first, ==, 1u);
    rc_arena_release(&a);
}


RC_TEST(arena, init_twice_is_noop)
{
    rc_arena a = rc_arena_make(PAGE);
    bool initialised = rc_arena_init();
    rc_arena b = rc_arena_make(PAGE);

    RC_CHECK_TRUE(initialised);

    // The heap carried on, not restarted
    RC_CHECK(b.first, ==, a.first + 1u);
    rc_arena_release(&a);
    rc_arena_release(&b);
}


// Deinit the page heap, then make a default arena, which starts it again.
static rc_arena deinit_and_make_default(void)
{
    rc_arena_deinit();
    return rc_arena_make_default();
}


RC_TEST(arena, make_after_deinit_reinits)
{
    rc_arena a = deinit_and_make_default();
    uint8_t *p = alloc_filled(&a, 1000, 8);

    RC_CHECK_TRUE(check_fill(p, 1000, 8));
    rc_arena_release(&a);
}


// Release a and start the page heap afresh, returning whether it started.
static bool release_and_restart_heap(rc_arena *a)
{
    rc_arena_release(a);
    rc_arena_deinit();
    return rc_arena_init();
}


RC_TEST(arena, deinit_empties_pool)
{
    // a's chunk is pooled, between two arenas, which the restart forgets along with it
    rc_arena low = rc_arena_make(PAGE);
    rc_arena a = rc_arena_make(7 * PAGE);
    rc_arena blocker = rc_arena_make(PAGE);
    bool restarted = release_and_restart_heap(&a);
    rc_arena b = rc_arena_make(PAGE);

    RC_CHECK(low.first, ==, 1u);
    RC_CHECK(blocker.first, ==, 9u);
    RC_CHECK_TRUE(restarted);

    // From the pool this would be page 2; fresh, it is the first page
    RC_CHECK(b.first, ==, 1u);
    rc_arena_release(&b);
}


RC_TEST(arena, deinit_twice_is_noop)
{
    rc_arena_deinit();
    rc_arena_deinit();
    RC_CHECK_TRUE(rc_arena_init());
}


// ---- the pool ----

RC_TEST(arena, pool_cuts_from_the_front_of_a_run)
{
    rc_arena a = rc_arena_make(7 * PAGE);
    rc_arena blocker = rc_arena_make(PAGE);
    uint32_t first = a.first;
    rc_arena b = release_and_make(&a, 2 * PAGE);
    rc_arena c = rc_arena_make(5 * PAGE);
    rc_arena d = rc_arena_make(PAGE);

    // Two pages from the front of the seven, then the other five, and then the pool is empty
    RC_CHECK(b.first, ==, first);
    RC_CHECK(b.end, ==, 2u * PAGE);
    RC_CHECK(c.first, ==, first + 2u);
    RC_CHECK(c.end, ==, 5u * PAGE);
    RC_CHECK(d.first, ==, blocker.first + 1u);
    rc_arena_release(&b);
    rc_arena_release(&c);
    rc_arena_release(&d);
    rc_arena_release(&blocker);
}


// Release first, second and third, in that order, then make an arena of chunk_size.
static rc_arena release_three_and_make(rc_arena *first, rc_arena *second, rc_arena *third, uint32_t chunk_size)
{
    rc_arena_release(first);
    rc_arena_release(second);
    return release_and_make(third, chunk_size);
}


RC_TEST(arena, pool_merges_neighbours)
{
    // The middle one goes last, and merges with both sides at once
    rc_arena a = rc_arena_make(2 * PAGE);
    rc_arena b = rc_arena_make(3 * PAGE);
    rc_arena c = rc_arena_make(2 * PAGE);
    rc_arena blocker = rc_arena_make(PAGE);
    uint32_t first = a.first;
    rc_arena d = release_three_and_make(&a, &c, &b, 7 * PAGE);

    // Only all three together hold seven pages
    RC_CHECK(d.first, ==, first);
    RC_CHECK(d.end, ==, 7u * PAGE);
    RC_CHECK(blocker.first, ==, first + 7u);
    rc_arena_release(&d);
    rc_arena_release(&blocker);
}


// Release first, then make an arena of chunk_size after releasing second.
static rc_arena release_two_and_make(rc_arena *first, rc_arena *second, uint32_t chunk_size)
{
    rc_arena_release(first);
    return release_and_make(second, chunk_size);
}


RC_TEST(arena, release_at_the_top_gives_pages_back)
{
    // b is at the top, so it goes back to the heap, and a after it, as it then reaches the top
    rc_arena a = rc_arena_make(2 * PAGE);
    rc_arena b = rc_arena_make(3 * PAGE);
    uint32_t first = a.first;
    rc_arena c = release_two_and_make(&b, &a, PAGE);
    uint8_t *p = alloc_filled(&c, 20 * PAGE, 3);

    // c starts where a did, at the top of the heap, so its chunk grows in place
    RC_CHECK(c.first, ==, first);
    RC_CHECK(c.chunk, ==, first);
    RC_CHECK_TRUE(check_fill(p, 20 * PAGE, 3));
    rc_arena_release(&c);
}


// Release other, then grow p, the last allocation in a, from size to new_size.
static uint8_t *release_and_grow(rc_arena *other, rc_arena *a, uint8_t *p, uint32_t size, uint32_t new_size)
{
    rc_arena_release(other);
    return rc_arena_realloc(a, p, size, new_size);
}


RC_TEST(arena, chunk_grows_into_a_released_neighbour)
{
    rc_arena a = rc_arena_make(PAGE);
    rc_arena b = rc_arena_make(4 * PAGE);
    rc_arena blocker = rc_arena_make(PAGE);
    uint8_t *p = alloc_filled(&a, PAGE / 2, 4);
    uint8_t *grown = release_and_grow(&b, &a, p, PAGE / 2, 2 * PAGE);
    rc_arena c = rc_arena_make(PAGE);

    // a took the two pages it needed from b's four, leaving the rest free after it
    RC_CHECK_TRUE(grown == p);
    RC_CHECK(a.chunk, ==, a.first);
    RC_CHECK(a.end, ==, 3u * PAGE);
    RC_CHECK_TRUE(check_fill(grown, PAGE / 2, 4));
    RC_CHECK(c.first, ==, a.first + 3u);
    rc_arena_release(&a);
    rc_arena_release(&c);
    rc_arena_release(&blocker);
}


// Reset a, and grow a block in it, from empty to size, in steps, returning where it ended up.
static uint8_t *reset_and_grow(rc_arena *a, uint32_t size)
{
    uint8_t *p = NULL;

    rc_arena_reset(a);
    for (uint32_t have = 0; have < size; have += PAGE) {
        p = rc_arena_realloc(a, p, have, have + PAGE);
    }
    return p;
}


RC_TEST(arena, reset_arena_regrows_in_place)
{
    // a grows to 10 pages, then b fills the pages after it; once reset, a grows the same block
    // again within its own chunk, with no new pages and no copies
    rc_arena a = rc_arena_make(PAGE);
    uint8_t *first = reset_and_grow(&a, 10 * PAGE);
    rc_arena b = rc_arena_make(PAGE);
    uint8_t *filler = alloc_filled(&b, 10 * PAGE, 5);
    uint8_t *again = reset_and_grow(&a, 10 * PAGE);

    RC_CHECK_TRUE(again == first);
    RC_CHECK(a.chunk, ==, a.first);
    RC_CHECK_TRUE(check_fill(filler, 10 * PAGE, 5));
    rc_arena_release(&b);
    rc_arena_release(&a);
}


// ---- giving pages back to the OS ----
//
// Linux and Windows hand back zeroed pages once they have taken them, so the zero pages in a chunk
// filled before its release are the ones that went back. macOS takes them lazily, and wasm not at
// all, so these tests are skipped there.

#if defined(__linux__) || defined(_WIN32)
#  define RC_TEST_DECOMMIT RC_TEST
#else
#  define RC_TEST_DECOMMIT RC_TEST_SKIP
#endif

// Free pages kept committed, as arena.c has it.
enum {
    RETAINED = 256
};


// True if the second half of the page at index page of chunk is all zeros. The first half is left
// out, as headers come and go there.
static bool is_zeroed_page(const uint8_t *chunk, uint32_t page)
{
    const uint8_t *half = chunk + (size_t)page * PAGE + PAGE / 2;

    for (uint32_t i = 0; i < PAGE / 2; i++) {
        if (half[i] != 0) {
            return false;
        }
    }
    return true;
}


// How many of the pages pages from chunk are not zeroed, when they are the first ones and the
// rest are; RC_INDEX_NONE if a zeroed page comes before one that is not.
static uint32_t pages_before_zeroed(const uint8_t *chunk, uint32_t pages)
{
    uint32_t front = 0;

    while (front < pages && !is_zeroed_page(chunk, front)) {
        front++;
    }

    for (uint32_t page = front; page < pages; page++) {
        if (!is_zeroed_page(chunk, page)) {
            return RC_INDEX_NONE;
        }
    }
    return front;
}


// A new arena of pages pages, its first chunk filled with 0xAB.
static rc_arena filled_arena(uint32_t pages)
{
    rc_arena a = rc_arena_make(pages * PAGE);

    rc_memset(rc_arena_alloc(&a, pages * PAGE - 16), 0xAB, pages * PAGE - 16);
    return a;
}


// How many pages at the front of a's first chunk, of pages pages, which is empty, still hold what
// was there before, with the rest zeroed.
static uint32_t kept_pages(rc_arena *a, uint32_t pages)
{
    return pages_before_zeroed((const uint8_t *)rc_arena_alloc(a, 16) - 16, pages);
}


// kept_pages of a new arena of pages pages, which is released again.
static uint32_t kept_pages_of_new_arena(uint32_t pages)
{
    rc_arena a = rc_arena_make(pages * PAGE);
    uint32_t kept = kept_pages(&a, pages);

    rc_arena_release(&a);
    return kept;
}


RC_TEST_DECOMMIT(arena, few_free_pages_stay_committed)
{
    rc_arena low = rc_arena_make(PAGE);
    rc_arena a = filled_arena(40);
    rc_arena blocker = rc_arena_make(PAGE);

    rc_arena_release(&a);
    RC_CHECK(kept_pages_of_new_arena(40), ==, 40u);
    rc_arena_release(&low);
    rc_arena_release(&blocker);
}


RC_TEST_DECOMMIT(arena, pool_gives_back_pages_past_those_retained)
{
    rc_arena low = rc_arena_make(PAGE);
    rc_arena a = filled_arena(300);
    rc_arena blocker = rc_arena_make(PAGE);

    rc_arena_release(&a);
    RC_CHECK(kept_pages_of_new_arena(300), ==, (uint32_t)RETAINED);
    rc_arena_release(&low);
    rc_arena_release(&blocker);
}


RC_TEST_DECOMMIT(arena, top_gives_back_pages_past_those_retained)
{
    rc_arena a = filled_arena(300);

    rc_arena_release(&a);
    RC_CHECK(kept_pages_of_new_arena(300), ==, (uint32_t)RETAINED);
}


RC_TEST_DECOMMIT(arena, top_gives_back_before_the_pool)
{
    // a's pages are in the pool when b's go back to the top, and the top gives back the excess;
    // then c takes a's run from the pool, and d the top
    rc_arena a = filled_arena(200);
    rc_arena blocker = rc_arena_make(PAGE);
    rc_arena b = filled_arena(200);
    rc_arena c = release_two_and_make(&a, &b, 200 * PAGE);
    rc_arena d = rc_arena_make(200 * PAGE);

    RC_CHECK(kept_pages(&c, 200), ==, 200u);
    RC_CHECK(kept_pages(&d, 200), ==, (uint32_t)RETAINED - 200u);
    rc_arena_release(&c);
    rc_arena_release(&d);
    rc_arena_release(&blocker);
}


RC_TEST_DECOMMIT(arena, merging_after_a_trimmed_run_gives_back_the_new_pages)
{
    // b's pages come after a's run, which no longer has all its pages, so they all go back
    rc_arena a = filled_arena(300);
    rc_arena b = filled_arena(4);
    rc_arena blocker = rc_arena_make(PAGE);
    rc_arena c = release_two_and_make(&a, &b, 304 * PAGE);

    RC_CHECK(kept_pages(&c, 304), ==, (uint32_t)RETAINED);
    rc_arena_release(&c);
    rc_arena_release(&blocker);
}


RC_TEST_DECOMMIT(arena, merging_before_a_trimmed_run_keeps_the_front)
{
    // a's pages come before b's run, so they and b's first ones are the front of the new run
    rc_arena a = filled_arena(4);
    rc_arena b = filled_arena(300);
    rc_arena blocker = rc_arena_make(PAGE);
    rc_arena c = release_two_and_make(&b, &a, 304 * PAGE);

    RC_CHECK(kept_pages(&c, 304), ==, (uint32_t)RETAINED);
    rc_arena_release(&c);
    rc_arena_release(&blocker);
}


RC_TEST_DECOMMIT(arena, top_merges_spare_pages)
{
    // b's first pages are spare at the top, then a's go back below them, and the excess goes from
    // the end of b's
    rc_arena a = filled_arena(4);
    rc_arena b = filled_arena(300);
    rc_arena c = release_two_and_make(&b, &a, 304 * PAGE);

    RC_CHECK(kept_pages(&c, 304), ==, (uint32_t)RETAINED);
    rc_arena_release(&c);
}


enum {
    STRESS_ARENAS = 6,
    STRESS_BLOCKS = 12,
    STRESS_STEPS = 2000
};

// A block a stress test has filled, to check later.
typedef struct stress_block {
    uint8_t *p;
    uint32_t size;
    uint8_t seed;
} stress_block;

// An arena in the stress test, and the blocks in it, while it is made.
typedef struct stress_arena {
    rc_arena arena;
    stress_block blocks[STRESS_BLOCKS];
    uint32_t num;
} stress_arena;


// The number of s's blocks that no longer hold their pattern.
static uint32_t stress_damaged(const stress_arena *s)
{
    uint32_t damaged = 0;

    for (uint32_t i = 0; i < s->num; i++) {
        damaged += check_fill(s->blocks[i].p, s->blocks[i].size, s->blocks[i].seed) ? 0u : 1u;
    }
    return damaged;
}


// A random block size: mostly up to a page, sometimes up to three.
static uint32_t stress_size(rc_random *random)
{
    uint32_t limit = rc_random_next(random) % 8 == 0 ? 3 * PAGE : PAGE;

    return 16 + rc_random_next(random) % limit;
}


// Grow s's last block by a random amount, refilling it.
static void stress_grow(stress_arena *s, rc_random *random)
{
    stress_block *last = &s->blocks[s->num - 1];
    uint32_t size = last->size + stress_size(random);

    last->p = rc_arena_realloc(&s->arena, last->p, last->size, size);
    last->size = size;
    fill(last->p, size, last->seed);
}


// Add a block to s, or grow its last one, at random.
static void stress_alloc(stress_arena *s, rc_random *random)
{
    bool grow = s->num == STRESS_BLOCKS || (s->num > 0 && rc_random_next(random) % 3 == 0);
    uint32_t size = stress_size(random);
    uint8_t seed = (uint8_t)rc_random_next(random);

    if (grow) {
        stress_grow(s, random);
        return;
    }

    s->blocks[s->num] = (stress_block) {.p = alloc_filled(&s->arena, size, seed), .size = size, .seed = seed};
    s->num++;
}


// Release s, or reset it, after checking its blocks. Returns how many were damaged.
static uint32_t stress_clear(stress_arena *s, bool release)
{
    uint32_t damaged = stress_damaged(s);

    if (release) {
        rc_arena_release(&s->arena);
    }
    else {
        rc_arena_reset(&s->arena);
    }

    s->num = 0;
    return damaged;
}


// One random step on one of the arenas: make it, release it, reset it, or allocate in it. Returns
// how many blocks were found damaged.
static uint32_t stress_step(stress_arena *arenas, rc_random *random)
{
    stress_arena *s = &arenas[rc_random_next(random) % STRESS_ARENAS];
    uint32_t action = rc_random_next(random) % 16;

    if (s->arena.first == 0) {
        s->arena = rc_arena_make((1 + action % 4) * PAGE);
        return 0;
    }

    if (action < 2) {
        return stress_clear(s, action == 0);
    }

    stress_alloc(s, random);
    return 0;
}


// Release every arena still made, returning how many blocks were found damaged.
static uint32_t stress_release_all(stress_arena *arenas)
{
    uint32_t damaged = 0;

    for (uint32_t i = 0; i < STRESS_ARENAS; i++) {
        damaged += arenas[i].arena.first != 0 ? stress_clear(&arenas[i], true) : 0u;
    }
    return damaged;
}


// The first page of a new one-page arena, which is released again.
static uint32_t first_page_of_new_arena(void)
{
    rc_arena a = rc_arena_make(PAGE);
    uint32_t first = a.first;

    rc_arena_release(&a);
    return first;
}


RC_TEST(arena, stress_many_arenas)
{
    // Arenas made, grown, reset and released at random, interleaved in the heap, never damage one
    // another's blocks; and once all are released, every page is back at the top
    stress_arena arenas[STRESS_ARENAS] = {0};
    rc_random random = rc_random_make(12345);
    uint32_t damaged = 0;

    for (uint32_t step = 0; step < STRESS_STEPS; step++) {
        damaged += stress_step(arenas, &random);
    }

    damaged += stress_release_all(arenas);
    RC_CHECK(damaged, ==, 0u);
    RC_CHECK(first_page_of_new_arena(), ==, 1u);
}

// ---- alloc ----

RC_TEST_STEP(arena, alloc_basic, fix)
{
    uint32_t top = fix->a.top;
    uint8_t *p = rc_arena_alloc(&fix->a, 100);

    RC_CHECK_TRUE(p != NULL);

    // 100 rounded up to a multiple of 16
    RC_CHECK(fix->a.top, ==, top + 112u);
    fill(p, 100, 1);
    RC_CHECK_TRUE(check_fill(p, 100, 1));
}


RC_TEST_STEP(arena, alloc_alignment, fix)
{
    uint8_t *p1 = rc_arena_alloc(&fix->a, 1);
    uint8_t *p2 = rc_arena_alloc(&fix->a, 1);

    for (uint32_t size = 1; size <= 100; size++) {
        RC_CHECK_TRUE(is_aligned(rc_arena_alloc(&fix->a, size), RC_MAX_ALIGN));
    }

    // A 1-byte request costs exactly one alignment unit
    RC_CHECK((uint32_t)(p2 - p1), ==, RC_MAX_ALIGN);
}


RC_TEST_STEP(arena, alloc_consecutive_do_not_overlap, fix)
{
    uint8_t *ptrs[50] = {0};

    for (uint32_t i = 0; i < 50; i++) {
        ptrs[i] = rc_arena_alloc(&fix->a, 3000);
        fill(ptrs[i], 3000, (uint8_t)i);
    }
    for (uint32_t i = 0; i < 50; i++) {
        RC_CHECK_TRUE(check_fill(ptrs[i], 3000, (uint8_t)i));
    }
}


RC_TEST_STEP(arena, alloc_zero, fix)
{
    uint8_t *p = alloc_dirty_and_free(&fix->a, 64, 0xAA);
    uint8_t *q = rc_arena_alloc_zero(&fix->a, 64);

    // The same memory, reused
    RC_CHECK_TRUE(q == p);
    for (uint32_t i = 0; i < 64; i++) {
        RC_CHECK(q[i], ==, (uint8_t)0);
    }
}


RC_TEST_STEP(arena, alloc_fills_chunk_exactly, fix)
{
    uint32_t chunk = fix->a.chunk;
    uint32_t room = fix->a.end - fix->a.top;
    uint8_t *p = rc_arena_alloc(&fix->a, room);

    RC_CHECK(fix->a.chunk, ==, chunk);
    RC_CHECK(fix->a.top, ==, fix->a.end);
    fill(p, room, 3);
    RC_CHECK_TRUE(check_fill(p, room, 3));
}


RC_TEST(arena, alloc_spills_into_new_chunk)
{
    rc_arena a = rc_arena_make(PAGE);
    rc_arena blocker = rc_arena_make(PAGE);
    uint32_t first = a.chunk;
    uint32_t room = a.end - a.top;
    uint8_t *p = alloc_filled(&a, room - 64, 5);
    uint8_t *q = alloc_filled(&a, 1000, 9);

    // q was too big for what was left, so it started a fresh chunk
    RC_CHECK(a.chunk, !=, first);
    RC_CHECK(a.top, ==, RC_MAX_ALIGN + 1008u);
    RC_CHECK_TRUE(check_fill(p, room - 64, 5));
    RC_CHECK_TRUE(check_fill(q, 1000, 9));
    rc_arena_release(&a);
    rc_arena_release(&blocker);
}


RC_TEST(arena, alloc_larger_than_chunk_size)
{
    rc_arena a = rc_arena_make_default();
    rc_arena blocker = rc_arena_make(PAGE);
    uint8_t *p = alloc_filled(&a, 5 * PAGE, 11);

    RC_CHECK(a.end - RC_MAX_ALIGN, >=, 5u * PAGE);
    RC_CHECK_TRUE(check_fill(p, 5 * PAGE, 11));
    rc_arena_release(&a);
    rc_arena_release(&blocker);
}


// ---- aligned alloc ----

RC_TEST_STEP(arena, alloc_aligned_small_align_is_plain_alloc, fix)
{
    uint32_t top = fix->a.top;
    uint8_t *p = rc_arena_alloc_aligned(&fix->a, 10, 8);

    RC_CHECK_TRUE(is_aligned(p, RC_MAX_ALIGN));
    RC_CHECK(fix->a.top, ==, top + RC_MAX_ALIGN);
}


RC_TEST_STEP(arena, alloc_aligned_various, fix)
{
    static const uint32_t aligns[] = {32, 64, 128, 256, 4096, 65536};

    for (uint32_t i = 0; i < RC_COUNT(aligns); i++) {
        // A 1-byte block first knocks top off alignment, so padding is needed
        uint8_t *p = alloc_aligned_after(&fix->a, 1, 100, aligns[i]);
        uint8_t *q = rc_arena_alloc(&fix->a, 16);

        RC_CHECK_TRUE(is_aligned(p, aligns[i]));
        fill(p, 100, (uint8_t)i);
        RC_CHECK_TRUE(check_fill(p, 100, (uint8_t)i));

        // A plain allocation after it is still aligned and does not overlap
        RC_CHECK_TRUE(is_aligned(q, RC_MAX_ALIGN));
        RC_CHECK_TRUE(q >= p + 100 || q < p);
    }
}


RC_TEST_STEP(arena, alloc_aligned_needs_new_room, fix)
{
    // Leave less room than the padding needs
    uint8_t *p = alloc_aligned_after(&fix->a, fix->a.end - fix->a.top - 32, 1000, 4096);

    RC_CHECK_TRUE(is_aligned(p, 4096));
    fill(p, 1000, 7);
    RC_CHECK_TRUE(check_fill(p, 1000, 7));
}


RC_TEST_STEP(arena, alloc_aligned_then_free, fix)
{
    uint8_t *p = alloc_aligned_after(&fix->a, 1, 64, 256);

    // The block itself can be freed; its padding stays used
    RC_CHECK_TRUE(rc_arena_free(&fix->a, p, 64));
    RC_CHECK_TRUE(rc_arena_alloc(&fix->a, 64) == p);
}


// ---- free ----

RC_TEST_STEP(arena, free_last, fix)
{
    uint32_t top = fix->a.top;
    void *p = rc_arena_alloc(&fix->a, 64);

    RC_CHECK_TRUE(rc_arena_free(&fix->a, p, 64));
    RC_CHECK(fix->a.top, ==, top);
}


RC_TEST_STEP(arena, free_last_unaligned_size, fix)
{
    uint32_t top = fix->a.top;
    void *p = rc_arena_alloc(&fix->a, 33);

    RC_CHECK_TRUE(rc_arena_free(&fix->a, p, 33));
    RC_CHECK(fix->a.top, ==, top);
}


RC_TEST_STEP(arena, free_not_last, fix)
{
    uint8_t *p = alloc_buried(&fix->a, 64, 0);
    uint32_t top = fix->a.top;

    RC_CHECK_FALSE(rc_arena_free(&fix->a, p, 64));
    RC_CHECK(fix->a.top, ==, top);
}


RC_TEST_STEP(arena, free_wrong_size, fix)
{
    void *p = rc_arena_alloc(&fix->a, 64);
    uint32_t top = fix->a.top;

    RC_CHECK_FALSE(rc_arena_free(&fix->a, p, 32));
    RC_CHECK_FALSE(rc_arena_free(&fix->a, p, 128));
    RC_CHECK(fix->a.top, ==, top);
}


RC_TEST_STEP(arena, free_in_earlier_chunk, fix)
{
    rc_arena blocker = rc_arena_make(PAGE);
    void *p = rc_arena_alloc(&fix->a, 64);

    // Fill the rest of the chunk and move on
    rc_arena_alloc(&fix->a, fix->a.end - fix->a.top);
    rc_arena_alloc(&fix->a, 64);
    RC_CHECK_FALSE(rc_arena_free(&fix->a, p, 64));
    rc_arena_release(&blocker);
}


RC_TEST_STEP(arena, free_twice_in_a_row, fix)
{
    uint32_t top = fix->a.top;
    void *p1 = rc_arena_alloc(&fix->a, 32);
    void *p2 = rc_arena_alloc(&fix->a, 48);

    RC_CHECK_TRUE(rc_arena_free(&fix->a, p2, 48));
    RC_CHECK_TRUE(rc_arena_free(&fix->a, p1, 32));
    RC_CHECK(fix->a.top, ==, top);
}


// ---- realloc ----

RC_TEST_STEP(arena, realloc_null_is_alloc, fix)
{
    void *p = rc_arena_realloc(&fix->a, NULL, 0, 100);

    RC_CHECK_TRUE(p != NULL);
    RC_CHECK_TRUE(is_aligned(p, RC_MAX_ALIGN));
}


RC_TEST_STEP(arena, realloc_grow_in_place, fix)
{
    uint8_t *p = alloc_filled(&fix->a, 32, 1);
    uint8_t *q = rc_arena_realloc(&fix->a, p, 32, 96);

    RC_CHECK_TRUE(q == p);
    RC_CHECK_TRUE(check_fill(q, 32, 1));
}


RC_TEST_STEP(arena, realloc_shrink_in_place, fix)
{
    uint8_t *p = rc_arena_alloc(&fix->a, 128);
    uint32_t top = fix->a.top;
    uint8_t *q = rc_arena_realloc(&fix->a, p, 128, 32);

    RC_CHECK_TRUE(q == p);

    // The tail is reclaimed
    RC_CHECK(fix->a.top, ==, top - 96u);
}


RC_TEST_STEP(arena, realloc_same_size, fix)
{
    uint8_t *p = rc_arena_alloc(&fix->a, 40);
    uint32_t top = fix->a.top;

    // 40 and 48 round to the same aligned size
    RC_CHECK_TRUE(rc_arena_realloc(&fix->a, p, 40, 48) == p);
    RC_CHECK(fix->a.top, ==, top);
}


RC_TEST_STEP(arena, realloc_move, fix)
{
    uint8_t *p = alloc_buried(&fix->a, 16, 1);
    uint8_t *q = rc_arena_realloc(&fix->a, p, 16, 64);

    RC_CHECK_TRUE(q != p);
    RC_CHECK_TRUE(check_fill(q, 16, 1));
}


RC_TEST_STEP(arena, realloc_shrink_not_last, fix)
{
    uint8_t *p = alloc_buried(&fix->a, 128, 0);
    uint32_t top = fix->a.top;

    // Shrinking a block that is not the last is a no-op
    RC_CHECK_TRUE(rc_arena_realloc(&fix->a, p, 128, 32) == p);
    RC_CHECK(fix->a.top, ==, top);
}


RC_TEST_STEP(arena, realloc_zero_grows, fix)
{
    uint8_t *p = alloc_filled(&fix->a, 16, 0xF0);
    uint8_t *q = rc_arena_realloc_zero(&fix->a, p, 16, 48);

    RC_CHECK_TRUE(q == p);
    RC_CHECK_TRUE(check_fill(q, 16, 0xF0));
    for (uint32_t i = 16; i < 48; i++) {
        RC_CHECK(q[i], ==, (uint8_t)0);
    }
}


RC_TEST_STEP(arena, realloc_zero_moves, fix)
{
    uint8_t *p = alloc_buried(&fix->a, 20, 0x10);
    uint8_t *dirty = alloc_dirty_and_free(&fix->a, 200, 0xEE);
    uint8_t *q = rc_arena_realloc_zero(&fix->a, p, 20, 200);

    // The block moves onto the dirty memory, which is zeroed past the copy
    RC_CHECK_TRUE(q != p);
    RC_CHECK_TRUE(q == dirty);
    RC_CHECK_TRUE(check_fill(q, 20, 0x10));
    for (uint32_t i = 20; i < 200; i++) {
        RC_CHECK(q[i], ==, (uint8_t)0);
    }
}


RC_TEST_STEP(arena, realloc_zero_null_is_alloc_zero, fix)
{
    uint8_t *dirty = alloc_dirty_and_free(&fix->a, 64, 0xAB);
    uint8_t *p = rc_arena_realloc_zero(&fix->a, NULL, 0, 64);

    RC_CHECK_TRUE(p == dirty);
    for (uint32_t i = 0; i < 64; i++) {
        RC_CHECK(p[i], ==, (uint8_t)0);
    }
}


RC_TEST_STEP(arena, realloc_last_past_chunk_end_moves, fix)
{
    rc_arena blocker = rc_arena_make(PAGE);
    uint32_t room = fix->a.end - fix->a.top;
    uint8_t *p = alloc_filled(&fix->a, room - 128, 4);
    uint8_t *q = rc_arena_realloc(&fix->a, p, room - 128, room + 1000);

    RC_CHECK_TRUE(q != p);
    RC_CHECK_TRUE(check_fill(q, 1000, 4));
    rc_arena_release(&blocker);
}


// ---- scratch, marks and reset ----

// Allocate from scratch, an arena passed by value, as a function taking a scratch arena does.
static uint8_t *scratch_alloc(rc_arena scratch, uint32_t size)
{
    return rc_arena_alloc(&scratch, size);
}


RC_TEST_STEP(arena, scratch_leaves_caller_untouched, fix)
{
    rc_arena before = fix->a;

    scratch_alloc(fix->a, 1000);
    RC_CHECK(fix->a.chunk, ==, before.chunk);
    RC_CHECK(fix->a.top, ==, before.top);
    RC_CHECK(fix->a.end, ==, before.end);
}


RC_TEST_STEP(arena, scratch_reuses_caller_memory, fix)
{
    uint8_t *s = scratch_alloc(fix->a, 1000);
    uint8_t *p = rc_arena_alloc(&fix->a, 1000);

    RC_CHECK_TRUE(p == s);
}


RC_TEST_STEP(arena, scratch_spill_is_reused_by_caller, fix)
{
    // Whether the scratch grew our chunk or linked a new one after it, the caller's next allocation
    // of the same size lands where the scratch's did
    uint8_t *s = scratch_alloc(fix->a, 2 * PAGE);
    uint8_t *p = rc_arena_alloc(&fix->a, 2 * PAGE);

    RC_CHECK_TRUE(p == s);
}


RC_TEST_STEP(arena, save_and_restore, fix)
{
    rc_arena_mark mark = rc_arena_save(&fix->a);
    uint8_t *p = rc_arena_alloc(&fix->a, 4096);

    rc_arena_restore(&fix->a, mark);
    RC_CHECK(fix->a.chunk, ==, mark.chunk);
    RC_CHECK(fix->a.top, ==, mark.top);
    RC_CHECK_TRUE(rc_arena_alloc(&fix->a, 4096) == p);
}


// Allocate size bytes, then mark the arena just after them.
static rc_arena_mark mark_after(rc_arena *arena, uint32_t size)
{
    rc_arena_alloc(arena, size);
    return rc_arena_save(arena);
}


RC_TEST_STEP(arena, restore_across_chunks, fix)
{
    rc_arena blocker = rc_arena_make(PAGE);
    rc_arena_mark mark = mark_after(&fix->a, 100);
    uint8_t *p = rc_arena_alloc(&fix->a, 100);

    for (uint32_t i = 0; i < 10; i++) {
        rc_arena_alloc(&fix->a, PAGE / 2);
    }
    RC_CHECK(fix->a.chunk, !=, mark.chunk);

    rc_arena_restore(&fix->a, mark);
    RC_CHECK_TRUE(rc_arena_alloc(&fix->a, 100) == p);
    rc_arena_release(&blocker);
}


RC_TEST_STEP(arena, reset_rewinds_to_start, fix)
{
    uint8_t *first = rc_arena_alloc(&fix->a, 10);

    rc_arena_alloc(&fix->a, 3 * PAGE);
    rc_arena_reset(&fix->a);
    RC_CHECK(fix->a.chunk, ==, fix->a.first);
    RC_CHECK(fix->a.top, ==, RC_MAX_ALIGN);
    RC_CHECK_TRUE(rc_arena_alloc(&fix->a, 10) == first);
}


RC_TEST_STEP(arena, reset_replays_same_addresses, fix)
{
    static const uint32_t sizes[] = {100, 40000, 30000, 70000, 16, 50000, 65000};
    rc_arena blocker = rc_arena_make(PAGE);
    uint8_t *ptrs[RC_COUNT(sizes)] = {0};

    // The chain built the first time round is walked again after reset
    for (uint32_t i = 0; i < RC_COUNT(sizes); i++) {
        ptrs[i] = rc_arena_alloc(&fix->a, sizes[i]);
    }
    rc_arena_reset(&fix->a);
    for (uint32_t i = 0; i < RC_COUNT(sizes); i++) {
        RC_CHECK_TRUE(rc_arena_alloc(&fix->a, sizes[i]) == ptrs[i]);
    }
    rc_arena_release(&blocker);
}


RC_TEST(arena, reset_zero_arena_is_noop)
{
    rc_arena a = {0};

    rc_arena_reset(&a);
    RC_CHECK(a.first, ==, 0u);
}


RC_TEST_STEP(arena, pointer_stability, fix)
{
    uint8_t *first = rc_arena_alloc(&fix->a, 8);

    first[0] = 0xAB;
    for (uint32_t k = 0; k < 1000; k++) {
        rc_arena_alloc(&fix->a, 1024);
    }
    RC_CHECK(first[0], ==, (uint8_t)0xAB);
}


// ---- out of memory ----

static jmp_buf oom_exit;
static uint32_t oom_size;

// The out-of-memory handler: note the size asked for and jump back into the test.
static void on_oom(uint32_t size)
{
    oom_size = size;
    longjmp(oom_exit, 1);
}


RC_TEST_STEP(arena, oom_handler_alloc, fix)
{
    oom_size = 0;
    rc_arena_set_oom_handler(on_oom);
    if (!setjmp(oom_exit)) {
        rc_arena_alloc(&fix->a, 0xFFFFFFFFu);
    }

    rc_arena_set_oom_handler(NULL);
    RC_CHECK(oom_size, ==, 0xFFFFFFFFu);
}


RC_TEST_STEP(arena, oom_handler_realloc, fix)
{
    void *p = rc_arena_alloc(&fix->a, 16);

    oom_size = 0;
    rc_arena_set_oom_handler(on_oom);
    if (!setjmp(oom_exit)) {
        rc_arena_realloc(&fix->a, p, 16, 0xFFFFFFF0u);
    }

    rc_arena_set_oom_handler(NULL);
    RC_CHECK(oom_size, ==, 0xFFFFFFF0u);
}


RC_TEST_STEP(arena, oom_handler_alloc_aligned, fix)
{
    oom_size = 0;
    rc_arena_set_oom_handler(on_oom);
    if (!setjmp(oom_exit)) {
        rc_arena_alloc_aligned(&fix->a, 0xFFFF0000u, 65536);
    }

    rc_arena_set_oom_handler(NULL);
    RC_CHECK(oom_size, ==, 0xFFFF0000u);
}


RC_TEST_STEP(arena, oom_leaves_arena_usable, fix)
{
    uint32_t top = fix->a.top;

    rc_arena_set_oom_handler(on_oom);
    if (!setjmp(oom_exit)) {
        rc_arena_alloc(&fix->a, 0xFFFFFFFFu);
    }

    rc_arena_set_oom_handler(NULL);
    RC_CHECK(fix->a.top, ==, top);
    RC_CHECK_TRUE(rc_arena_alloc(&fix->a, 100) != NULL);
}


// ---- chunk growth ----

RC_TEST(arena, alloc_extends_newest_chunk)
{
    rc_arena a = rc_arena_make(4 * PAGE);
    uint32_t first = a.first;
    uint32_t end = a.end;
    uint8_t *p = alloc_filled(&a, end - a.top, 2);
    uint8_t *q = rc_arena_alloc(&a, 1000);

    // Grew in place, with no new chunk
    RC_CHECK(a.chunk, ==, first);
    RC_CHECK(a.end, >, end);
    RC_CHECK_TRUE(q == p + (end - RC_MAX_ALIGN));
    RC_CHECK_TRUE(check_fill(p, 1000, 2));
    rc_arena_release(&a);
}


RC_TEST(arena, realloc_growing_array_stays_put)
{
    rc_arena a = rc_arena_make(0);
    uint32_t cap = 1024;
    uint8_t *p = alloc_filled(&a, cap, 6);

    // Doubling all the way to 64 MiB never moves it: the chunk grows instead
    while (cap < (64u << 20)) {
        uint8_t *q = rc_arena_realloc(&a, p, cap, cap * 2);

        RC_CHECK_TRUE(q == p);
        cap *= 2;
    }
    RC_CHECK_TRUE(check_fill(p, 1024, 6));
    RC_CHECK(a.chunk, ==, a.first);

    // The array, plus the page its header shares, and every byte of it writable
    RC_CHECK(a.end, ==, (64u << 20) + (uint32_t)PAGE);
    p[cap - 1] = 0x5A;
    RC_CHECK(p[cap - 1], ==, (uint8_t)0x5A);
    rc_arena_release(&a);
}
