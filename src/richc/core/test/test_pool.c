// test_pool.c
//
// Tests for the pool template.

#include "richc/test.h"

#define RC_POOL_TYPE int32_t
#define RC_POOL_NAME i32
#include "richc/pool.template.h"

typedef struct point {
    int32_t x;
    int32_t y;
} point;

// With no RC_POOL_NAME, the type's own name is the suffix
#define RC_POOL_TYPE point
#include "richc/pool.template.h"

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(pool) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(pool, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(pool, fix)
{
    rc_arena_release(&fix->a);
}


// ---- lifecycle ----

RC_TEST_STEP(pool, make, fix)
{
    rc_pool_i32 pool = rc_pool_i32_make(8, &fix->a);

    RC_CHECK(pool.slots_.num, ==, 0u);
    RC_CHECK(pool.slots_.cap, ==, 8u);
}


RC_TEST_STEP(pool, zero_is_empty, fix)
{
    rc_pool_i32 pool = {0};

    RC_CHECK(rc_pool_i32_add(&pool, 0, &fix->a), ==, 0u);
    RC_CHECK(rc_pool_i32_add(&pool, 0, &fix->a), ==, 1u);
}


RC_TEST_STEP(pool, reserve, fix)
{
    rc_pool_i32 pool = {0};

    rc_pool_i32_reserve(&pool, 20, &fix->a);
    RC_CHECK(pool.slots_.cap, ==, 20u);
    rc_pool_i32_reserve(&pool, 10, NULL);
    RC_CHECK(pool.slots_.cap, ==, 20u);
}


RC_TEST_STEP(pool, reset, fix)
{
    rc_pool_i32 pool = {0};

    rc_pool_i32_add(&pool, 0, &fix->a);
    rc_pool_i32_remove(&pool, rc_pool_i32_add(&pool, 0, &fix->a));
    rc_pool_i32_reset(&pool);
    RC_CHECK(pool.slots_.num, ==, 0u);
    RC_CHECK(pool.first_free_, ==, 0u);
    RC_CHECK(rc_pool_i32_add(&pool, 0, &fix->a), ==, 0u);
}


RC_TEST_STEP(pool, release, fix)
{
    rc_pool_i32 pool = rc_pool_i32_make(8, &fix->a);

    rc_pool_i32_add(&pool, 0, &fix->a);
    rc_pool_i32_release(&pool, &fix->a);
    RC_CHECK(pool.slots_.num, ==, 0u);
    RC_CHECK(pool.slots_.cap, ==, 0u);
    RC_CHECK_TRUE(pool.slots_.data == NULL);
    RC_CHECK(pool.first_free_, ==, 0u);

    // The zeroed pool works again
    RC_CHECK(rc_pool_i32_add(&pool, 0, &fix->a), ==, 0u);
}


// ---- add and remove ----

RC_TEST_STEP(pool, add_stores_value, fix)
{
    rc_pool_i32 pool = {0};
    uint32_t i = rc_pool_i32_add(&pool, 42, &fix->a);

    RC_CHECK(rc_pool_i32_get(&pool, i), ==, 42);
}


RC_TEST_STEP(pool, remove_then_reuse, fix)
{
    rc_pool_i32 pool = {0};

    rc_pool_i32_add(&pool, 0, &fix->a);

    uint32_t b = rc_pool_i32_add(&pool, 0, &fix->a);
    rc_pool_i32_add(&pool, 0, &fix->a);
    rc_pool_i32_set(&pool, b, 7);
    rc_pool_i32_remove(&pool, b);
    RC_CHECK(rc_pool_i32_num_slots(&pool), ==, 3u);

    // The freed slot takes the next value
    RC_CHECK(rc_pool_i32_add(&pool, 8, &fix->a), ==, b);
    RC_CHECK(rc_pool_i32_get(&pool, b), ==, 8);
}


RC_TEST_STEP(pool, reuses_last_freed_first, fix)
{
    rc_pool_i32 pool = {0};

    for (uint32_t i = 0; i < 4; i++) {
        rc_pool_i32_add(&pool, 0, &fix->a);
    }
    rc_pool_i32_remove(&pool, 1);
    rc_pool_i32_remove(&pool, 2);
    RC_CHECK(rc_pool_i32_add(&pool, 0, &fix->a), ==, 2u);
    RC_CHECK(rc_pool_i32_add(&pool, 0, &fix->a), ==, 1u);
    RC_CHECK(rc_pool_i32_add(&pool, 0, &fix->a), ==, 4u);
}


RC_TEST_STEP(pool, remove_last_slot_keeps_it, fix)
{
    // Removing from the final slot does not shrink the pool
    rc_pool_i32 pool = {0};

    for (uint32_t i = 0; i < 3; i++) {
        rc_pool_i32_add(&pool, 0, &fix->a);
    }
    rc_pool_i32_remove(&pool, 2);
    RC_CHECK(pool.slots_.num, ==, 3u);
    RC_CHECK(rc_pool_i32_add(&pool, 0, &fix->a), ==, 2u);
}


RC_TEST_STEP(pool, index_zero_round_trips, fix)
{
    // Slot 0 is stored as link 1 on the free list
    rc_pool_i32 pool = {0};

    rc_pool_i32_add(&pool, 0, &fix->a);
    rc_pool_i32_add(&pool, 0, &fix->a);
    rc_pool_i32_remove(&pool, 0);
    RC_CHECK(pool.first_free_, ==, 1u);
    RC_CHECK(rc_pool_i32_add(&pool, 0, &fix->a), ==, 0u);
    RC_CHECK(pool.first_free_, ==, 0u);
}


RC_TEST_STEP(pool, add_remove_are_inverses, fix)
{
    // Whatever shape the free list has, add then remove puts it back
    rc_pool_i32 pool = {0};

    for (uint32_t i = 0; i < 3; i++) {
        rc_pool_i32_add(&pool, 0, &fix->a);
    }
    rc_pool_i32_remove(&pool, 1);
    rc_pool_i32_remove(&pool, 2);
    for (uint32_t step = 0; step < 6; step++) {
        uint32_t num = pool.slots_.num;
        uint32_t first_free = pool.first_free_;

        rc_pool_i32_remove(&pool, rc_pool_i32_add(&pool, 0, &fix->a));
        RC_CHECK(pool.slots_.num, ==, num);
        RC_CHECK(pool.first_free_, ==, first_free);
    }
}


RC_TEST_STEP(pool, growth_and_reuse, fix)
{
    rc_pool_i32 pool = {0};

    for (uint32_t i = 0; i < 100; i++) {
        RC_CHECK(rc_pool_i32_add(&pool, (int32_t)(i * 10), &fix->a), ==, i);
    }
    for (uint32_t i = 0; i < 100; i += 2) {
        rc_pool_i32_remove(&pool, i);
    }

    // The 50 freed slots are reused before the pool grows; the rest are untouched
    for (uint32_t i = 0; i < 50; i++) {
        uint32_t index = rc_pool_i32_add(&pool, -1, &fix->a);

        RC_CHECK(index % 2, ==, 0u);
        RC_CHECK(rc_pool_i32_get(&pool, index), ==, -1);
    }
    RC_CHECK(pool.slots_.num, ==, 100u);
    for (uint32_t i = 1; i < 100; i += 2) {
        RC_CHECK(rc_pool_i32_get(&pool, i), ==, (int32_t)(i * 10));
    }
}


// ---- access ----

RC_TEST_STEP(pool, get_set_at, fix)
{
    rc_pool_i32 pool = {0};
    uint32_t i = rc_pool_i32_add(&pool, 0, &fix->a);

    rc_pool_i32_set(&pool, i, 42);
    RC_CHECK(rc_pool_i32_get(&pool, i), ==, 42);
    *rc_pool_i32_at(&pool, i) = 99;
    RC_CHECK(rc_pool_i32_get(&pool, i), ==, 99);
    RC_CHECK(*rc_pool_i32_at_const(&pool, i), ==, 99);
}


RC_TEST_STEP(pool, pointer_survives_other_frees, fix)
{
    rc_pool_i32 pool = {0};
    uint32_t a = rc_pool_i32_add(&pool, 0, &fix->a);
    uint32_t b = rc_pool_i32_add(&pool, 0, &fix->a);
    int32_t *pb = rc_pool_i32_at(&pool, b);

    *pb = 1234;
    rc_pool_i32_remove(&pool, a);
    RC_CHECK_TRUE(pb == rc_pool_i32_at(&pool, b));
    RC_CHECK(rc_pool_i32_get(&pool, b), ==, 1234);
}


RC_TEST_STEP(pool, struct_elements, fix)
{
    rc_pool_point pool = {0};
    uint32_t i = rc_pool_point_add(&pool, (point) {.x = 1, .y = 2}, &fix->a);

    RC_CHECK(rc_pool_point_get(&pool, i).x, ==, 1);
    RC_CHECK(rc_pool_point_get(&pool, i).y, ==, 2);
    rc_pool_point_set(&pool, i, (point) {.x = 3, .y = 4});
    RC_CHECK(rc_pool_point_get(&pool, i).y, ==, 4);
    rc_pool_point_at(&pool, i)->x = 5;
    RC_CHECK(rc_pool_point_get(&pool, i).x, ==, 5);
}


// ---- iteration ----

RC_TEST_STEP(pool, free_bitset, fix)
{
    rc_pool_i32 pool = {0};

    for (uint32_t i = 0; i < 8; i++) {
        rc_pool_i32_add(&pool, 0, &fix->a);
    }
    rc_pool_i32_remove(&pool, 2);
    rc_pool_i32_remove(&pool, 5);

    rc_bitset free_slots = rc_pool_i32_free_bitset(&pool, &fix->a);
    RC_CHECK(free_slots.num, ==, rc_pool_i32_num_slots(&pool));
    for (uint32_t i = 0; i < 8; i++) {
        RC_CHECK(rc_bitset_is_set(&free_slots, i), ==, (i == 2 || i == 5));
    }
}


RC_TEST(pool, free_bitset_empty)
{
    rc_pool_i32 pool = {0};
    rc_bitset free_slots = rc_pool_i32_free_bitset(&pool, NULL);

    RC_CHECK(free_slots.num, ==, 0u);
}
