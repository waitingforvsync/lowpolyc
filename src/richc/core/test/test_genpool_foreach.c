// test_genpool_foreach.c
//
// Tests for the genpool foreach template.

#include "richc/arena.h"
#include "richc/test.h"

#define RC_GENPOOL_TYPE int32_t
#define RC_GENPOOL_NAME i32
#include "richc/genpool.template.h"

// What a walk saw, in order
typedef struct visits {
    uint32_t num;
    rc_handle_i32 handles[16];
    int32_t values[16];
} visits;

// Where the walk without a context records what it sees
static visits seen;


// Record the element with handle h into v.
static void visit(visits *v, const rc_genpool_i32 *pool, rc_handle_i32 h)
{
    RC_ASSERT(v->num < RC_COUNT(v->handles));
    v->handles[v->num] = h;
    v->values[v->num] = rc_genpool_i32_get(pool, h);
    v->num++;
}


// Record the element with handle h into v, then remove it.
static void visit_and_remove(visits *v, rc_genpool_i32 *pool, rc_handle_i32 h)
{
    visit(v, pool, h);
    rc_genpool_i32_remove(pool, h);
}


#define RC_GENPOOL_FOREACH_POOL rc_genpool_i32
#define RC_GENPOOL_FOREACH_FUNC(p, h) visit(&seen, p, h)
#include "richc/algorithm/genpool_foreach.template.h"

#define RC_GENPOOL_FOREACH_POOL rc_genpool_i32
#define RC_GENPOOL_FOREACH_CTX visits
#define RC_GENPOOL_FOREACH_CONST
#define RC_GENPOOL_FOREACH_FUNC(v, p, h) visit(v, p, h)
#define RC_GENPOOL_FOREACH_NAME foreach_const
#include "richc/algorithm/genpool_foreach.template.h"

#define RC_GENPOOL_FOREACH_POOL rc_genpool_i32
#define RC_GENPOOL_FOREACH_CTX visits
#define RC_GENPOOL_FOREACH_FUNC(v, p, h) visit_and_remove(v, p, h)
#define RC_GENPOOL_FOREACH_NAME foreach_removing
#include "richc/algorithm/genpool_foreach.template.h"

// Each test runs against a fresh default arena, with nothing seen yet
RC_TEST_GROUP_DATA(genpool_foreach) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(genpool_foreach, fix)
{
    fix->a = rc_arena_make_default();
    seen = (visits) {0};
}


RC_TEST_GROUP_DEINIT(genpool_foreach, fix)
{
    rc_arena_release(&fix->a);
}


// A genpool of 10 to 14 in slots 0 to 4, with 11 and 13 removed again.
static rc_genpool_i32 pool_with_gaps(rc_arena *arena)
{
    rc_genpool_i32 pool = {0};
    rc_handle_i32 handles[5] = {{0}};

    for (uint32_t i = 0; i < RC_COUNT(handles); i++) {
        handles[i] = rc_genpool_i32_add(&pool, 10 + (int32_t)i, arena);
    }
    rc_genpool_i32_remove(&pool, handles[1]);
    rc_genpool_i32_remove(&pool, handles[3]);
    return pool;
}


RC_TEST_STEP(genpool_foreach, empty, fix)
{
    rc_genpool_i32 none = {0};
    rc_genpool_i32 reserved = rc_genpool_i32_make(8, &fix->a);

    rc_genpool_i32_foreach(&none);
    rc_genpool_i32_foreach(&reserved);
    RC_CHECK(seen.num, ==, 0u);
}


RC_TEST_STEP(genpool_foreach, live_elements_in_order, fix)
{
    rc_genpool_i32 pool = pool_with_gaps(&fix->a);

    rc_genpool_i32_foreach(&pool);
    RC_CHECK(seen.num, ==, 3u);
    RC_CHECK(seen.values[0], ==, 10);
    RC_CHECK(seen.values[1], ==, 12);
    RC_CHECK(seen.values[2], ==, 14);

    // The handles are the live ones, so still valid
    RC_CHECK_TRUE(rc_genpool_i32_is_valid(&pool, seen.handles[0]));
    RC_CHECK_TRUE(rc_genpool_i32_is_valid(&pool, seen.handles[2]));
}


RC_TEST_STEP(genpool_foreach, const_with_context, fix)
{
    rc_genpool_i32 pool = pool_with_gaps(&fix->a);
    const rc_genpool_i32 *read_only = &pool;
    visits v = {0};

    foreach_const(read_only, &v);
    RC_CHECK(v.num, ==, 3u);
    RC_CHECK(v.values[1], ==, 12);
    RC_CHECK(seen.num, ==, 0u);
}


RC_TEST_STEP(genpool_foreach, removing_each_element, fix)
{
    rc_genpool_i32 pool = pool_with_gaps(&fix->a);
    visits removed = {0};
    visits after = {0};

    foreach_removing(&pool, &removed);
    RC_CHECK(removed.num, ==, 3u);
    RC_CHECK_FALSE(rc_genpool_i32_is_valid(&pool, removed.handles[0]));

    // Nothing is left
    foreach_const(&pool, &after);
    RC_CHECK(after.num, ==, 0u);
}
