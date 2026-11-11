// test_arena.c
//
// Tests for the chunked arena.
//
// The runner gives each test a fresh page heap, so with the pool empty every chunk an arena makes
// is new at the end of the heap. A "blocker" arena made after another stops that one's chunk from
// growing in place.

#include "richc/arena.h"

#include <setjmp.h>

#include "richc/test.h"

enum {
    PAGE = 65536
};

static bool is_aligned(const void *p, uint32_t align)
{
    return (uintptr_t)p % align == 0;
}


// Fill with a pattern derived from seed, and check it back.
static void fill(uint8_t *p, uint32_t size, uint8_t seed)
{
    for (uint32_t i = 0; i < size; i++) {
        p[i] = (uint8_t)(seed + i);
    }
}


static bool check_fill(const uint8_t *p, uint32_t size, uint8_t seed)
{
    for (uint32_t i = 0; i < size; i++) {
        if (p[i] != (uint8_t)(seed + i)) {
            return false;
        }
    }
    return true;
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

    rc_arena_release(&a);
    rc_arena b = rc_arena_make(7 * PAGE);
    RC_CHECK(b.first, ==, first);
    rc_arena_release(&b);
}


RC_TEST(arena, release_recycles_whole_chain)
{
    rc_arena a = rc_arena_make(8 * PAGE);
    rc_arena blocker = rc_arena_make(PAGE);   // stops a's chunk growing in place

    // Spill into a second chunk
    rc_arena_alloc(&a, 8 * PAGE - RC_MAX_ALIGN);
    rc_arena_alloc(&a, 1000);
    uint32_t second = a.chunk;
    RC_CHECK(second, !=, a.first);

    rc_arena_release(&a);
    rc_arena b = rc_arena_make(8 * PAGE);
    rc_arena c = rc_arena_make(8 * PAGE);
    RC_CHECK(b.first, ==, second);   // the pool is a stack: last chunk in, first out
    RC_CHECK(c.first, !=, 0u);
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

    RC_CHECK(a.first, ==, 1u);   // the first page the heap hands out
    rc_arena_release(&a);
}


RC_TEST(arena, init_twice_is_noop)
{
    rc_arena a = rc_arena_make(PAGE);

    RC_CHECK_TRUE(rc_arena_init());
    rc_arena b = rc_arena_make(PAGE);
    RC_CHECK(b.first, ==, a.first + 1u);   // the heap carried on, not restarted
    rc_arena_release(&a);
    rc_arena_release(&b);
}


RC_TEST(arena, make_after_deinit_reinits)
{
    rc_arena_deinit();

    rc_arena a = rc_arena_make_default();
    uint8_t *p = rc_arena_alloc(&a, 1000);
    fill(p, 1000, 8);
    RC_CHECK_TRUE(check_fill(p, 1000, 8));
    rc_arena_release(&a);
}


RC_TEST(arena, deinit_empties_pool)
{
    rc_arena a = rc_arena_make(7 * PAGE);

    rc_arena_release(&a);
    rc_arena_deinit();
    RC_CHECK_TRUE(rc_arena_init());

    // From the pool this would be the 7-page chunk; fresh, it is one page
    rc_arena b = rc_arena_make(PAGE);
    RC_CHECK(b.end, ==, (uint32_t)PAGE);
    rc_arena_release(&b);
}


RC_TEST(arena, deinit_twice_is_noop)
{
    rc_arena_deinit();
    rc_arena_deinit();
    RC_CHECK_TRUE(rc_arena_init());
}


// ---- alloc ----

RC_TEST_STEP(arena, alloc_basic, fix)
{
    uint32_t top = fix->a.top;
    uint8_t *p = rc_arena_alloc(&fix->a, 100);

    RC_CHECK_TRUE(p != NULL);
    RC_CHECK(fix->a.top, ==, top + 112u);   // 100 rounded up to 16
    fill(p, 100, 1);
    RC_CHECK_TRUE(check_fill(p, 100, 1));
}


RC_TEST_STEP(arena, alloc_alignment, fix)
{
    for (uint32_t size = 1; size <= 100; size++) {
        RC_CHECK_TRUE(is_aligned(rc_arena_alloc(&fix->a, size), RC_MAX_ALIGN));
    }

    // A 1-byte request costs exactly one alignment unit
    uint8_t *p1 = rc_arena_alloc(&fix->a, 1);
    uint8_t *p2 = rc_arena_alloc(&fix->a, 1);
    RC_CHECK((uint32_t)(p2 - p1), ==, RC_MAX_ALIGN);
}


RC_TEST_STEP(arena, alloc_consecutive_do_not_overlap, fix)
{
    uint8_t *ptrs[50];

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
    uint8_t *p = rc_arena_alloc(&fix->a, 64);

    fill(p, 64, 0xAA);
    RC_CHECK_TRUE(rc_arena_free(&fix->a, p, 64));

    uint8_t *q = rc_arena_alloc_zero(&fix->a, 64);
    RC_CHECK_TRUE(q == p);   // same memory, reused
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
    rc_arena blocker = rc_arena_make(PAGE);   // a's chunk is no longer the newest

    uint32_t first = a.chunk;
    uint32_t room = a.end - a.top;
    uint8_t *p = rc_arena_alloc(&a, room - 64);
    fill(p, room - 64, 5);

    // Too big for what is left, so it starts a fresh chunk
    uint8_t *q = rc_arena_alloc(&a, 1000);
    RC_CHECK(a.chunk, !=, first);
    RC_CHECK(a.top, ==, RC_MAX_ALIGN + 1008u);
    fill(q, 1000, 9);
    RC_CHECK_TRUE(check_fill(p, room - 64, 5));
    RC_CHECK_TRUE(check_fill(q, 1000, 9));
    rc_arena_release(&a);
    rc_arena_release(&blocker);
}


RC_TEST(arena, alloc_larger_than_chunk_size)
{
    rc_arena a = rc_arena_make_default();
    rc_arena blocker = rc_arena_make(PAGE);

    uint8_t *p = rc_arena_alloc(&a, 5 * PAGE);
    RC_CHECK(a.end - RC_MAX_ALIGN, >=, 5u * PAGE);
    fill(p, 5 * PAGE, 11);
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

    for (uint32_t i = 0; i < sizeof aligns / sizeof aligns[0]; i++) {

        // Knock top off alignment first, so padding is needed
        rc_arena_alloc(&fix->a, 1);
        uint8_t *p = rc_arena_alloc_aligned(&fix->a, 100, aligns[i]);
        RC_CHECK_TRUE(is_aligned(p, aligns[i]));
        fill(p, 100, (uint8_t)i);
        RC_CHECK_TRUE(check_fill(p, 100, (uint8_t)i));

        // A plain allocation after it is still aligned and does not overlap
        uint8_t *q = rc_arena_alloc(&fix->a, 16);
        RC_CHECK_TRUE(is_aligned(q, RC_MAX_ALIGN));
        RC_CHECK_TRUE(q >= p + 100 || q < p);
    }
}


RC_TEST_STEP(arena, alloc_aligned_needs_new_room, fix)
{
    // Leave less room than the padding needs
    rc_arena_alloc(&fix->a, fix->a.end - fix->a.top - 32);

    uint8_t *p = rc_arena_alloc_aligned(&fix->a, 1000, 4096);
    RC_CHECK_TRUE(is_aligned(p, 4096));
    fill(p, 1000, 7);
    RC_CHECK_TRUE(check_fill(p, 1000, 7));
}


RC_TEST_STEP(arena, alloc_aligned_then_free, fix)
{
    rc_arena_alloc(&fix->a, 1);
    uint8_t *p = rc_arena_alloc_aligned(&fix->a, 64, 256);

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
    void *p1 = rc_arena_alloc(&fix->a, 64);
    rc_arena_alloc(&fix->a, 64);
    uint32_t top = fix->a.top;

    RC_CHECK_FALSE(rc_arena_free(&fix->a, p1, 64));
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
    uint8_t *p = rc_arena_alloc(&fix->a, 32);

    fill(p, 32, 1);
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
    RC_CHECK(fix->a.top, ==, top - 96u);   // the tail is reclaimed
}


RC_TEST_STEP(arena, realloc_same_size, fix)
{
    uint8_t *p = rc_arena_alloc(&fix->a, 40);
    uint32_t top = fix->a.top;

    RC_CHECK_TRUE(rc_arena_realloc(&fix->a, p, 40, 48) == p);   // same aligned size
    RC_CHECK(fix->a.top, ==, top);
}


RC_TEST_STEP(arena, realloc_move, fix)
{
    uint8_t *p = rc_arena_alloc(&fix->a, 16);

    fill(p, 16, 1);
    rc_arena_alloc(&fix->a, 16);   // p is no longer the last block
    uint8_t *q = rc_arena_realloc(&fix->a, p, 16, 64);
    RC_CHECK_TRUE(q != p);
    RC_CHECK_TRUE(check_fill(q, 16, 1));
}


RC_TEST_STEP(arena, realloc_shrink_not_last, fix)
{
    uint8_t *p = rc_arena_alloc(&fix->a, 128);

    rc_arena_alloc(&fix->a, 16);
    uint32_t top = fix->a.top;
    RC_CHECK_TRUE(rc_arena_realloc(&fix->a, p, 128, 32) == p);   // a no-op
    RC_CHECK(fix->a.top, ==, top);
}


RC_TEST_STEP(arena, realloc_zero_grows, fix)
{
    uint8_t *p = rc_arena_alloc(&fix->a, 16);

    fill(p, 16, 0xF0);
    uint8_t *q = rc_arena_realloc_zero(&fix->a, p, 16, 48);
    RC_CHECK_TRUE(q == p);
    RC_CHECK_TRUE(check_fill(q, 16, 0xF0));
    for (uint32_t i = 16; i < 48; i++) {
        RC_CHECK(q[i], ==, (uint8_t)0);
    }
}


RC_TEST_STEP(arena, realloc_zero_moves, fix)
{
    uint8_t *p = rc_arena_alloc(&fix->a, 20);

    fill(p, 20, 0x10);
    uint8_t *blocker = rc_arena_alloc(&fix->a, 16);
    fill(blocker, 16, 0xFF);

    // Dirty the memory the moved block will land on, then free it again
    uint8_t *dirty = rc_arena_alloc(&fix->a, 200);
    fill(dirty, 200, 0xEE);
    rc_arena_free(&fix->a, dirty, 200);

    uint8_t *q = rc_arena_realloc_zero(&fix->a, p, 20, 200);
    RC_CHECK_TRUE(q != p);
    RC_CHECK_TRUE(check_fill(q, 20, 0x10));
    for (uint32_t i = 20; i < 200; i++) {
        RC_CHECK(q[i], ==, (uint8_t)0);
    }
}


RC_TEST_STEP(arena, realloc_zero_null_is_alloc_zero, fix)
{
    uint8_t *dirty = rc_arena_alloc(&fix->a, 64);

    fill(dirty, 64, 0xAB);
    rc_arena_free(&fix->a, dirty, 64);

    uint8_t *p = rc_arena_realloc_zero(&fix->a, NULL, 0, 64);
    RC_CHECK_TRUE(p == dirty);
    for (uint32_t i = 0; i < 64; i++) {
        RC_CHECK(p[i], ==, (uint8_t)0);
    }
}


RC_TEST_STEP(arena, realloc_last_past_chunk_end_moves, fix)
{
    rc_arena blocker = rc_arena_make(PAGE);   // our chunk cannot grow in place

    uint32_t room = fix->a.end - fix->a.top;
    uint8_t *p = rc_arena_alloc(&fix->a, room - 128);
    fill(p, 1000, 4);

    uint8_t *q = rc_arena_realloc(&fix->a, p, room - 128, room + 1000);
    RC_CHECK_TRUE(q != p);
    RC_CHECK_TRUE(check_fill(q, 1000, 4));
    rc_arena_release(&blocker);
}


// ---- scratch, marks and reset ----

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


RC_TEST_STEP(arena, restore_across_chunks, fix)
{
    rc_arena blocker = rc_arena_make(PAGE);

    rc_arena_alloc(&fix->a, 100);
    rc_arena_mark mark = rc_arena_save(&fix->a);
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
    rc_arena blocker = rc_arena_make(PAGE);
    static const uint32_t sizes[] = {100, 40000, 30000, 70000, 16, 50000, 65000};
    uint8_t *ptrs[sizeof sizes / sizeof sizes[0]];

    // The chain built the first time round is walked again after reset
    for (uint32_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        ptrs[i] = rc_arena_alloc(&fix->a, sizes[i]);
    }
    rc_arena_reset(&fix->a);
    for (uint32_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
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

    uint8_t *p = rc_arena_alloc(&a, end - a.top);
    fill(p, 1000, 2);
    uint8_t *q = rc_arena_alloc(&a, 1000);
    RC_CHECK(a.chunk, ==, first);   // grew in place, no new chunk
    RC_CHECK(a.end, >, end);
    RC_CHECK_TRUE(q == p + (end - RC_MAX_ALIGN));
    RC_CHECK_TRUE(check_fill(p, 1000, 2));
    rc_arena_release(&a);
}


RC_TEST(arena, realloc_growing_array_stays_put)
{
    rc_arena a = rc_arena_make(0);
    uint32_t cap = 1024;
    uint8_t *p = rc_arena_alloc(&a, cap);

    // Doubling all the way to 64 MiB never moves it: the chunk grows instead
    fill(p, cap, 6);
    while (cap < (64u << 20)) {
        uint8_t *q = rc_arena_realloc(&a, p, cap, cap * 2);
        RC_CHECK_TRUE(q == p);
        cap *= 2;
    }
    RC_CHECK_TRUE(check_fill(p, 1024, 6));
    RC_CHECK(a.chunk, ==, a.first);
    RC_CHECK(a.end, ==, (64u << 20) + (uint32_t)PAGE);   // the array, plus the page its header shares
    p[cap - 1] = 0x5A;   // the whole thing is writable
    RC_CHECK(p[cap - 1], ==, (uint8_t)0x5A);
    rc_arena_release(&a);
}
