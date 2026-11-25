// test_pool_foreach.c
//
// Tests for the pool foreach template.

#include "richc/arena.h"
#include "richc/test.h"

#define RC_POOL_TYPE int32_t
#define RC_POOL_NAME i32
#include "richc/pool.template.h"

// What a walk saw, in order
typedef struct visits {
    uint32_t num;
    uint32_t indices[16];
    int32_t values[16];
} visits;

// A walk that changes the pool as it goes, and the arena its adds grow it from
typedef struct changer {
    visits seen;
    rc_arena *arena;
} changer;

// Where the walk without a context records what it sees
static visits seen;


// Record the element at index into v.
static void visit(visits *v, const rc_pool_i32 *pool, uint32_t index)
{
    RC_ASSERT(v->num < RC_COUNT(v->indices));
    v->indices[v->num] = index;
    v->values[v->num] = rc_pool_i32_get(pool, index);
    v->num++;
}


// Record the element at index, then remove it.
static void visit_and_remove(changer *c, rc_pool_i32 *pool, uint32_t index)
{
    visit(&c->seen, pool, index);
    rc_pool_i32_remove(pool, index);
}


// Record the element at index, then add another.
static void visit_and_add(changer *c, rc_pool_i32 *pool, uint32_t index)
{
    visit(&c->seen, pool, index);
    rc_pool_i32_add(pool, -1, c->arena);
}


#define RC_POOL_FOREACH_POOL rc_pool_i32
#define RC_POOL_FOREACH_FUNC(p, i) visit(&seen, p, i)
#include "richc/algorithm/pool_foreach.template.h"

#define RC_POOL_FOREACH_POOL rc_pool_i32
#define RC_POOL_FOREACH_CTX visits
#define RC_POOL_FOREACH_CONST
#define RC_POOL_FOREACH_FUNC(v, p, i) visit(v, p, i)
#define RC_POOL_FOREACH_NAME foreach_const
#include "richc/algorithm/pool_foreach.template.h"

#define RC_POOL_FOREACH_POOL rc_pool_i32
#define RC_POOL_FOREACH_CTX changer
#define RC_POOL_FOREACH_FUNC(c, p, i) visit_and_remove(c, p, i)
#define RC_POOL_FOREACH_NAME foreach_removing
#include "richc/algorithm/pool_foreach.template.h"

#define RC_POOL_FOREACH_POOL rc_pool_i32
#define RC_POOL_FOREACH_CTX changer
#define RC_POOL_FOREACH_FUNC(c, p, i) visit_and_add(c, p, i)
#define RC_POOL_FOREACH_NAME foreach_adding
#include "richc/algorithm/pool_foreach.template.h"

// Each test runs against a fresh default arena, with nothing seen yet
RC_TEST_GROUP_DATA(pool_foreach) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(pool_foreach, fix)
{
    fix->a = rc_arena_make_default();
    seen = (visits) {0};
}


RC_TEST_GROUP_DEINIT(pool_foreach, fix)
{
    rc_arena_release(&fix->a);
}


// A pool of 10 to 14 at indices 0 to 4, with 11 and 13 removed again.
static rc_pool_i32 pool_with_gaps(rc_arena *arena)
{
    rc_pool_i32 pool = {0};

    for (int32_t value = 10; value < 15; value++) {
        rc_pool_i32_add(&pool, value, arena);
    }
    rc_pool_i32_remove(&pool, 1);
    rc_pool_i32_remove(&pool, 3);
    return pool;
}


RC_TEST_STEP(pool_foreach, empty, fix)
{
    rc_pool_i32 pool = {0};

    rc_pool_i32_foreach(&pool, fix->a);
    RC_CHECK(seen.num, ==, 0u);
}


RC_TEST_STEP(pool_foreach, live_elements_in_order, fix)
{
    rc_pool_i32 pool = pool_with_gaps(&fix->a);

    rc_pool_i32_foreach(&pool, fix->a);
    RC_CHECK(seen.num, ==, 3u);
    RC_CHECK(seen.indices[0], ==, 0u);
    RC_CHECK(seen.indices[1], ==, 2u);
    RC_CHECK(seen.indices[2], ==, 4u);
    RC_CHECK(seen.values[0], ==, 10);
    RC_CHECK(seen.values[1], ==, 12);
    RC_CHECK(seen.values[2], ==, 14);
}


RC_TEST_STEP(pool_foreach, const_with_context, fix)
{
    rc_pool_i32 pool = pool_with_gaps(&fix->a);
    const rc_pool_i32 *read_only = &pool;
    visits v = {0};

    foreach_const(read_only, &v, fix->a);
    RC_CHECK(v.num, ==, 3u);
    RC_CHECK(v.values[2], ==, 14);
    RC_CHECK(seen.num, ==, 0u);
}


RC_TEST_STEP(pool_foreach, removing_each_element, fix)
{
    rc_pool_i32 pool = pool_with_gaps(&fix->a);
    changer c = {.arena = &fix->a};
    visits after = {0};

    foreach_removing(&pool, &c, fix->a);
    RC_CHECK(c.seen.num, ==, 3u);

    // Nothing is left
    foreach_const(&pool, &after, fix->a);
    RC_CHECK(after.num, ==, 0u);
}


RC_TEST_STEP(pool_foreach, adds_are_not_visited, fix)
{
    // The first two adds reuse the free slots, and the third grows the pool
    rc_pool_i32 pool = pool_with_gaps(&fix->a);
    changer c = {.arena = &fix->a};
    visits after = {0};

    foreach_adding(&pool, &c, fix->a);
    RC_CHECK(c.seen.num, ==, 3u);
    RC_CHECK(c.seen.values[0], ==, 10);
    RC_CHECK(c.seen.values[1], ==, 12);
    RC_CHECK(c.seen.values[2], ==, 14);

    foreach_const(&pool, &after, fix->a);
    RC_CHECK(after.num, ==, 6u);
}
