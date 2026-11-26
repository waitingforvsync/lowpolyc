// test_arena_ts.c
//
// Tests for rc_arena_ts, on one thread. Threads sharing one are in test_thread.c.

#include "richc/arena_ts.h"

#include "richc/mem.h"
#include "richc/test.h"

enum {
    PAGE = 65536
};

// True if the size bytes at p are all value.
static bool all_bytes(const uint8_t *p, uint32_t size, uint8_t value)
{
    for (uint32_t i = 0; i < size; i++) {
        if (p[i] != value) {
            return false;
        }
    }
    return true;
}


// size bytes from a, every one set to value.
static uint8_t *alloc_set(rc_arena_ts *a, uint32_t size, uint8_t value)
{
    uint8_t *p = rc_arena_ts_alloc(a, size);

    rc_memset(p, value, size);
    return p;
}


RC_TEST(arena_ts, make_default)
{
    rc_arena_ts a = rc_arena_ts_make_default();

    RC_CHECK(a.arena_.first, !=, 0u);
    RC_CHECK(a.arena_.end, ==, (uint32_t)RC_ARENA_DEFAULT_CHUNK);
    RC_CHECK_TRUE(rc_mutex_try_lock(&a.lock_));
    rc_mutex_unlock(&a.lock_);
    rc_arena_ts_release(&a);
}


RC_TEST(arena_ts, make_takes_a_chunk_size)
{
    rc_arena_ts a = rc_arena_ts_make(3 * PAGE);

    RC_CHECK(a.arena_.end, ==, 3u * PAGE);
    rc_arena_ts_release(&a);
}


RC_TEST(arena_ts, release_zeroes)
{
    rc_arena_ts a = rc_arena_ts_make_default();

    rc_arena_ts_alloc(&a, 100);
    rc_arena_ts_release(&a);
    RC_CHECK(a.arena_.first, ==, 0u);
    RC_CHECK(a.arena_.top, ==, 0u);
}


RC_TEST(arena_ts, alloc_does_not_overlap)
{
    rc_arena_ts a = rc_arena_ts_make_default();
    uint8_t *first = alloc_set(&a, 100, 0x11);
    uint8_t *second = alloc_set(&a, 100, 0x22);

    RC_CHECK_TRUE((uintptr_t)first % RC_MAX_ALIGN == 0);
    RC_CHECK_TRUE((uintptr_t)second % RC_MAX_ALIGN == 0);
    RC_CHECK_TRUE(all_bytes(first, 100, 0x11));
    RC_CHECK_TRUE(all_bytes(second, 100, 0x22));
    rc_arena_ts_release(&a);
}


RC_TEST(arena_ts, alloc_larger_than_a_chunk)
{
    rc_arena_ts a = rc_arena_ts_make(PAGE);
    uint8_t *small = alloc_set(&a, 16, 0x33);
    uint8_t *large = alloc_set(&a, 3 * PAGE, 0x44);

    RC_CHECK_TRUE(all_bytes(small, 16, 0x33));
    RC_CHECK_TRUE(all_bytes(large, 3 * PAGE, 0x44));
    rc_arena_ts_release(&a);
}


// Dirty size bytes at the top of a and free them again, so the next allocation lands on them.
static void dirty_top(rc_arena_ts *a, uint32_t size)
{
    uint8_t *p = alloc_set(a, size, 0xEE);

    RC_CHECK_TRUE(rc_arena_ts_free(a, p, size));
}


RC_TEST(arena_ts, alloc_zero_zeroes)
{
    rc_arena_ts a = rc_arena_ts_make_default();

    dirty_top(&a, 256);
    RC_CHECK_TRUE(all_bytes(rc_arena_ts_alloc_zero(&a, 256), 256, 0));
    rc_arena_ts_release(&a);
}


RC_TEST(arena_ts, alloc_aligned)
{
    rc_arena_ts a = rc_arena_ts_make_default();

    rc_arena_ts_alloc(&a, 16);
    for (uint32_t align = 32; align <= PAGE; align *= 2) {
        RC_CHECK_TRUE((uintptr_t)rc_arena_ts_alloc_aligned(&a, 16, align) % align == 0);
    }

    rc_arena_ts_release(&a);
}


RC_TEST(arena_ts, free_only_the_last)
{
    rc_arena_ts a = rc_arena_ts_make_default();
    uint8_t *first = rc_arena_ts_alloc(&a, 64);
    uint8_t *second = rc_arena_ts_alloc(&a, 64);

    RC_CHECK_FALSE(rc_arena_ts_free(&a, first, 64));
    RC_CHECK_TRUE(rc_arena_ts_free(&a, second, 64));
    RC_CHECK_TRUE(rc_arena_ts_alloc(&a, 64) == second);
    rc_arena_ts_release(&a);
}


RC_TEST(arena_ts, realloc_grows_the_last_in_place)
{
    rc_arena_ts a = rc_arena_ts_make_default();
    uint8_t *p = alloc_set(&a, 100, 0x55);
    uint8_t *grown = rc_arena_ts_realloc(&a, p, 100, 1000);

    RC_CHECK_TRUE(grown == p);
    RC_CHECK_TRUE(all_bytes(grown, 100, 0x55));
    rc_arena_ts_release(&a);
}


RC_TEST(arena_ts, realloc_moves_one_not_last)
{
    rc_arena_ts a = rc_arena_ts_make_default();
    uint8_t *p = alloc_set(&a, 100, 0x66);
    uint8_t *after = alloc_set(&a, 16, 0x77);
    uint8_t *moved = rc_arena_ts_realloc(&a, p, 100, 200);

    RC_CHECK_TRUE(moved != p);
    RC_CHECK_TRUE(all_bytes(moved, 100, 0x66));
    RC_CHECK_TRUE(all_bytes(after, 16, 0x77));
    rc_arena_ts_release(&a);
}


RC_TEST(arena_ts, realloc_null_allocates)
{
    rc_arena_ts a = rc_arena_ts_make_default();
    uint8_t *p = rc_arena_ts_realloc(&a, NULL, 0, 64);

    RC_CHECK_TRUE(p != NULL);
    rc_arena_ts_release(&a);
}


// A block of 100 bytes set to 0x88, with dirty memory after it, grown to 300 with zeroing.
static uint8_t *grow_zeroed_over_dirt(rc_arena_ts *a)
{
    uint8_t *p = alloc_set(a, 100, 0x88);

    dirty_top(a, 512);
    return rc_arena_ts_realloc_zero(a, p, 100, 300);
}


RC_TEST(arena_ts, realloc_zero_zeroes_what_it_adds)
{
    rc_arena_ts a = rc_arena_ts_make_default();
    uint8_t *p = grow_zeroed_over_dirt(&a);

    RC_CHECK_TRUE(all_bytes(p, 100, 0x88));
    RC_CHECK_TRUE(all_bytes(p + 100, 200, 0));
    rc_arena_ts_release(&a);
}


RC_TEST(arena_ts, realloc_zero_null_zeroes_it_all)
{
    rc_arena_ts a = rc_arena_ts_make_default();

    dirty_top(&a, 128);
    RC_CHECK_TRUE(all_bytes(rc_arena_ts_realloc_zero(&a, NULL, 0, 128), 128, 0));
    rc_arena_ts_release(&a);
}


RC_TEST(arena_ts, reset_rewinds_to_the_start)
{
    rc_arena_ts a = rc_arena_ts_make(PAGE);
    uint8_t *first = rc_arena_ts_alloc(&a, 64);

    rc_arena_ts_alloc(&a, 2 * PAGE);
    rc_arena_ts_reset(&a);
    RC_CHECK_TRUE(rc_arena_ts_alloc(&a, 64) == first);
    rc_arena_ts_release(&a);
}


// Allocate size bytes through the arena that locking a gives, returning the next allocation after
// it once unlocked.
static uint8_t *alloc_locked_then_after(rc_arena_ts *a, uint32_t size)
{
    rc_arena *arena = rc_arena_ts_lock(a);
    bool locked = !rc_mutex_try_lock(&a->lock_);

    rc_arena_alloc(arena, size);
    rc_arena_ts_unlock(a);
    RC_CHECK_TRUE(locked);
    return rc_arena_ts_alloc(a, size);
}


RC_TEST(arena_ts, lock_gives_the_arena_underneath)
{
    rc_arena_ts a = rc_arena_ts_make_default();
    uint8_t *before = rc_arena_ts_alloc(&a, 64);
    uint8_t *after = alloc_locked_then_after(&a, 64);

    // The locked allocation came between the other two
    RC_CHECK_TRUE(after == before + 128);
    RC_CHECK_TRUE(rc_mutex_try_lock(&a.lock_));
    rc_mutex_unlock(&a.lock_);
    rc_arena_ts_release(&a);
}
