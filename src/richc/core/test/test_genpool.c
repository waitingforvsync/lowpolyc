// test_genpool.c
//
// Tests for the genpool and handle templates.
//
// Everything that takes a bad handle asserts, which the test framework cannot catch, so only
// is_valid is tested with bad handles.

#include "richc/test.h"

// A genpool with its own handle type, rc_handle_i32
#define RC_GENPOOL_TYPE int32_t
#define RC_GENPOOL_NAME i32
#include "richc/genpool.template.h"

typedef struct point {
    int32_t x;
    int32_t y;
} point;

// A handle type made apart from its pool, as a public header would
#define RC_HANDLE_TYPE point_id
#include "richc/handle.template.h"

#define RC_GENPOOL_TYPE point
#define RC_GENPOOL_HANDLE point_id
#include "richc/genpool.template.h"

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(genpool) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(genpool, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(genpool, fix)
{
    rc_arena_release(&fix->a);
}


// ---- handles ----

RC_TEST(genpool, handle_none)
{
    rc_handle_i32 none = {0};
    rc_handle_i32 slot_zero = {.index_ = 0, .gen_ = 1};

    RC_CHECK_TRUE(rc_handle_i32_is_none(none));
    RC_CHECK_FALSE(rc_handle_i32_is_none(slot_zero));
}


RC_TEST(genpool, handle_is_equal)
{
    rc_handle_i32 h = {.index_ = 5, .gen_ = 7};

    RC_CHECK_TRUE(rc_handle_i32_is_equal(h, (rc_handle_i32) {.index_ = 5, .gen_ = 7}));
    RC_CHECK_FALSE(rc_handle_i32_is_equal(h, (rc_handle_i32) {.index_ = 6, .gen_ = 7}));
    RC_CHECK_FALSE(rc_handle_i32_is_equal(h, (rc_handle_i32) {.index_ = 5, .gen_ = 9}));
    RC_CHECK_TRUE(rc_handle_i32_is_equal((rc_handle_i32) {0}, (rc_handle_i32) {0}));
}


RC_TEST_STEP(genpool, separate_handle_type, fix)
{
    rc_genpool_point pool = {0};
    point_id h = rc_genpool_point_add(&pool, (point) {.x = 1, .y = 2}, &fix->a);

    RC_CHECK_FALSE(point_id_is_none(h));
    RC_CHECK_TRUE(rc_genpool_point_is_valid(&pool, h));
}


// ---- lifecycle ----

RC_TEST_STEP(genpool, make, fix)
{
    rc_genpool_i32 pool = rc_genpool_i32_make(8, &fix->a);

    RC_CHECK(rc_genpool_i32_num_slots(&pool), ==, 0u);
    RC_CHECK(pool.slots_.cap, ==, 8u);
}


RC_TEST_STEP(genpool, zero_is_empty, fix)
{
    rc_genpool_i32 pool = {0};
    rc_handle_i32 h = rc_genpool_i32_add(&pool, 0, &fix->a);

    RC_CHECK_TRUE(rc_genpool_i32_is_valid(&pool, h));
    RC_CHECK_TRUE(rc_handle_i32_is_equal(h, rc_genpool_i32_handle_at(&pool, 0)));
}


RC_TEST_STEP(genpool, reserve, fix)
{
    rc_genpool_i32 pool = {0};

    rc_genpool_i32_reserve(&pool, 20, &fix->a);
    RC_CHECK(pool.slots_.cap, ==, 20u);
    rc_genpool_i32_reserve(&pool, 10, NULL);
    RC_CHECK(pool.slots_.cap, ==, 20u);
}


RC_TEST_STEP(genpool, reset_restarts_generations, fix)
{
    // A handle from before the reset matches one from after it, as the header warns
    rc_genpool_i32 pool = {0};
    rc_handle_i32 before = rc_genpool_i32_add(&pool, 1, &fix->a);

    rc_genpool_i32_reset(&pool);
    RC_CHECK(rc_genpool_i32_num_slots(&pool), ==, 0u);
    RC_CHECK(pool.first_free_, ==, 0u);
    RC_CHECK_FALSE(rc_genpool_i32_is_valid(&pool, before));

    rc_handle_i32 after = rc_genpool_i32_add(&pool, 2, &fix->a);
    RC_CHECK_TRUE(rc_handle_i32_is_equal(before, after));
}


RC_TEST_STEP(genpool, release, fix)
{
    rc_genpool_i32 pool = rc_genpool_i32_make(8, &fix->a);

    rc_genpool_i32_add(&pool, 1, &fix->a);
    rc_genpool_i32_release(&pool, &fix->a);
    RC_CHECK(pool.slots_.num, ==, 0u);
    RC_CHECK(pool.slots_.cap, ==, 0u);
    RC_CHECK_TRUE(pool.slots_.data == NULL);
    RC_CHECK(pool.first_free_, ==, 0u);
    RC_CHECK_TRUE(rc_genpool_i32_is_valid(&pool, rc_genpool_i32_add(&pool, 2, &fix->a)));
}


// ---- add and remove ----

RC_TEST_STEP(genpool, add_sequential, fix)
{
    rc_genpool_i32 pool = {0};

    for (uint32_t i = 0; i < 3; i++) {
        rc_handle_i32 h = rc_genpool_i32_add(&pool, (int32_t)i + 10, &fix->a);

        RC_CHECK(h.index_, ==, i);
        RC_CHECK(h.gen_, ==, 1u);
        RC_CHECK(rc_genpool_i32_get(&pool, h), ==, (int32_t)i + 10);
    }
    RC_CHECK(rc_genpool_i32_num_slots(&pool), ==, 3u);
}


RC_TEST_STEP(genpool, remove_makes_handle_stale, fix)
{
    rc_genpool_i32 pool = {0};
    rc_handle_i32 h = rc_genpool_i32_add(&pool, 1, &fix->a);

    rc_genpool_i32_remove(&pool, h);
    RC_CHECK_FALSE(rc_genpool_i32_is_valid(&pool, h));
    RC_CHECK_FALSE(rc_genpool_i32_is_live(&pool, h.index_));
}


RC_TEST_STEP(genpool, generation_parity_tracks_liveness, fix)
{
    // add and remove each move the generation on by one: odd while live, even while free
    rc_genpool_i32 pool = {0};
    rc_handle_i32 h0 = rc_genpool_i32_add(&pool, 1, &fix->a);

    RC_CHECK(h0.gen_, ==, 1u);
    rc_genpool_i32_remove(&pool, h0);
    RC_CHECK(pool.slots_.data[0].gen, ==, 2u);

    rc_handle_i32 h1 = rc_genpool_i32_add(&pool, 2, &fix->a);
    RC_CHECK(h1.index_, ==, h0.index_);
    RC_CHECK(h1.gen_, ==, 3u);
    RC_CHECK_FALSE(rc_genpool_i32_is_valid(&pool, h0));
    RC_CHECK_TRUE(rc_genpool_i32_is_valid(&pool, h1));

    rc_genpool_i32_remove(&pool, h1);

    rc_handle_i32 h2 = rc_genpool_i32_add(&pool, 3, &fix->a);
    RC_CHECK(h2.gen_, ==, 5u);
    RC_CHECK_FALSE(rc_genpool_i32_is_valid(&pool, h1));
    RC_CHECK_TRUE(rc_genpool_i32_is_valid(&pool, h2));
}


RC_TEST_STEP(genpool, generation_wraps_to_free, fix)
{
    // The last odd generation wraps to 0 on remove, which is even, so the slot reads as free
    rc_genpool_i32 pool = {0};
    rc_handle_i32 h = rc_genpool_i32_add(&pool, 1, &fix->a);

    pool.slots_.data[0].gen = UINT32_MAX;
    h.gen_ = UINT32_MAX;
    rc_genpool_i32_remove(&pool, h);
    RC_CHECK(pool.slots_.data[0].gen, ==, 0u);
    RC_CHECK_FALSE(rc_genpool_i32_is_live(&pool, 0));
    RC_CHECK_FALSE(rc_genpool_i32_is_valid(&pool, (rc_handle_i32) {0}));
    RC_CHECK(rc_genpool_i32_add(&pool, 2, &fix->a).gen_, ==, 1u);
}


RC_TEST_STEP(genpool, reuses_last_freed_first, fix)
{
    rc_genpool_i32 pool = {0};
    rc_handle_i32 h[4];

    for (uint32_t i = 0; i < 4; i++) {
        h[i] = rc_genpool_i32_add(&pool, 0, &fix->a);
    }
    rc_genpool_i32_remove(&pool, h[1]);
    rc_genpool_i32_remove(&pool, h[2]);
    RC_CHECK(rc_genpool_i32_add(&pool, 0, &fix->a).index_, ==, h[2].index_);
    RC_CHECK(rc_genpool_i32_add(&pool, 0, &fix->a).index_, ==, h[1].index_);
    RC_CHECK(rc_genpool_i32_num_slots(&pool), ==, 4u);
}


RC_TEST_STEP(genpool, growth_and_reuse, fix)
{
    rc_genpool_i32 pool = {0};
    rc_handle_i32 h[100];

    for (uint32_t i = 0; i < 100; i++) {
        h[i] = rc_genpool_i32_add(&pool, (int32_t)(i * 10), &fix->a);
    }
    for (uint32_t i = 0; i < 100; i += 2) {
        rc_genpool_i32_remove(&pool, h[i]);
    }

    // The freed slots take the new values in a new generation; the others keep theirs
    for (uint32_t i = 0; i < 50; i++) {
        rc_handle_i32 r = rc_genpool_i32_add(&pool, -1, &fix->a);

        RC_CHECK(r.gen_, ==, 3u);
        RC_CHECK(rc_genpool_i32_get(&pool, r), ==, -1);
    }
    RC_CHECK(rc_genpool_i32_num_slots(&pool), ==, 100u);
    for (uint32_t i = 0; i < 100; i++) {
        if (i % 2 == 0) {
            RC_CHECK_FALSE(rc_genpool_i32_is_valid(&pool, h[i]));
        }
        else {
            RC_CHECK(rc_genpool_i32_get(&pool, h[i]), ==, (int32_t)(i * 10));
        }
    }
}


// ---- validity ----

RC_TEST_STEP(genpool, none_is_never_valid, fix)
{
    rc_genpool_i32 pool = {0};
    rc_handle_i32 none = {0};

    // A live slot 0 shows that none is not just slot 0
    rc_genpool_i32_add(&pool, 1, &fix->a);
    RC_CHECK_FALSE(rc_genpool_i32_is_valid(&pool, none));
}


RC_TEST_STEP(genpool, forged_handles_are_invalid, fix)
{
    rc_genpool_i32 pool = {0};
    rc_handle_i32 live = rc_genpool_i32_add(&pool, 1, &fix->a);
    rc_handle_i32 out_of_range = {.index_ = 100, .gen_ = 1};
    rc_handle_i32 wrong_gen = {.index_ = live.index_, .gen_ = 99};

    RC_CHECK_FALSE(rc_genpool_i32_is_valid(&pool, out_of_range));
    RC_CHECK_FALSE(rc_genpool_i32_is_valid(&pool, wrong_gen));
}


RC_TEST_STEP(genpool, even_generation_is_never_valid, fix)
{
    // A free slot's generation is even, and no handle with it validates
    rc_genpool_i32 pool = {0};
    rc_handle_i32 h = rc_genpool_i32_add(&pool, 1, &fix->a);

    rc_genpool_i32_remove(&pool, h);
    RC_CHECK_FALSE(rc_genpool_i32_is_valid(&pool, (rc_handle_i32) {.index_ = h.index_, .gen_ = 2}));
}


// ---- access ----

RC_TEST_STEP(genpool, get_set_at, fix)
{
    rc_genpool_i32 pool = {0};
    rc_handle_i32 h = rc_genpool_i32_add(&pool, 0, &fix->a);

    rc_genpool_i32_set(&pool, h, 42);
    RC_CHECK(rc_genpool_i32_get(&pool, h), ==, 42);
    *rc_genpool_i32_at(&pool, h) = 99;
    RC_CHECK(rc_genpool_i32_get(&pool, h), ==, 99);
    RC_CHECK(*rc_genpool_i32_at_const(&pool, h), ==, 99);
}


RC_TEST_STEP(genpool, struct_elements, fix)
{
    rc_genpool_point pool = {0};
    point_id h = rc_genpool_point_add(&pool, (point) {.x = 1, .y = 2}, &fix->a);

    RC_CHECK(rc_genpool_point_get(&pool, h).x, ==, 1);
    rc_genpool_point_set(&pool, h, (point) {.x = 3, .y = 4});
    RC_CHECK(rc_genpool_point_get(&pool, h).y, ==, 4);
    rc_genpool_point_at(&pool, h)->x = 5;
    RC_CHECK(rc_genpool_point_get(&pool, h).x, ==, 5);
    rc_genpool_point_remove(&pool, h);
    RC_CHECK_FALSE(rc_genpool_point_is_valid(&pool, h));
}


// ---- iteration ----

RC_TEST_STEP(genpool, is_live, fix)
{
    rc_genpool_i32 pool = {0};
    rc_handle_i32 h[8];

    for (uint32_t i = 0; i < 8; i++) {
        h[i] = rc_genpool_i32_add(&pool, 0, &fix->a);
    }
    rc_genpool_i32_remove(&pool, h[2]);
    rc_genpool_i32_remove(&pool, h[5]);
    for (uint32_t i = 0; i < 8; i++) {
        RC_CHECK(rc_genpool_i32_is_live(&pool, i), ==, (i != 2 && i != 5));
    }
}


RC_TEST_STEP(genpool, visit_live_elements, fix)
{
    // The pattern num_slots, is_live and handle_at are for: every live element, by handle
    rc_genpool_i32 pool = {0};
    rc_handle_i32 h[6];
    int32_t sum = 0;

    for (uint32_t i = 0; i < 6; i++) {
        h[i] = rc_genpool_i32_add(&pool, (int32_t)(i + 1), &fix->a);
    }
    rc_genpool_i32_remove(&pool, h[0]);
    rc_genpool_i32_remove(&pool, h[3]);
    for (uint32_t i = 0; i < rc_genpool_i32_num_slots(&pool); i++) {
        if (rc_genpool_i32_is_live(&pool, i)) {
            sum += rc_genpool_i32_get(&pool, rc_genpool_i32_handle_at(&pool, i));
        }
    }
    RC_CHECK(sum, ==, 2 + 3 + 5 + 6);
}


RC_TEST_STEP(genpool, handle_at, fix)
{
    rc_genpool_i32 pool = {0};
    rc_handle_i32 a = rc_genpool_i32_add(&pool, 1, &fix->a);
    rc_handle_i32 b = rc_genpool_i32_add(&pool, 2, &fix->a);

    rc_genpool_i32_remove(&pool, b);

    rc_handle_i32 b2 = rc_genpool_i32_add(&pool, 3, &fix->a);
    RC_CHECK_TRUE(rc_handle_i32_is_equal(rc_genpool_i32_handle_at(&pool, 0), a));
    RC_CHECK_TRUE(rc_handle_i32_is_equal(rc_genpool_i32_handle_at(&pool, 1), b2));
    RC_CHECK_FALSE(rc_handle_i32_is_equal(rc_genpool_i32_handle_at(&pool, 1), b));
}
