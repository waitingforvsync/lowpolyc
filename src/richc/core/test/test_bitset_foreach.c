// test_bitset_foreach.c
//
// Tests for the bitset foreach template.

#include "richc/arena.h"
#include "richc/test.h"

// What a walk saw, in order
typedef struct visits {
    uint32_t num;
    uint32_t indices[16];
} visits;

// Where the walk without a context records what it sees
static visits seen;


// Record index into v.
static void visit(visits *v, uint32_t index)
{
    RC_ASSERT(v->num < RC_COUNT(v->indices));
    v->indices[v->num] = index;
    v->num++;
}


#define RC_BITSET_FOREACH_FUNC(i) visit(&seen, i)
#include "richc/algorithm/bitset_foreach.template.h"

#define RC_BITSET_FOREACH_CTX visits
#define RC_BITSET_FOREACH_FUNC(v, i) visit(v, i)
#define RC_BITSET_FOREACH_NAME foreach_with_context
#include "richc/algorithm/bitset_foreach.template.h"

// Each test runs against a fresh default arena, with nothing seen yet
RC_TEST_GROUP_DATA(bitset_foreach) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(bitset_foreach, fix)
{
    fix->a = rc_arena_make_default();
    seen = (visits) {0};
}


RC_TEST_GROUP_DEINIT(bitset_foreach, fix)
{
    rc_arena_release(&fix->a);
}


// A bitset of 100 bits, with bits set either side of word boundaries and at both ends.
static rc_bitset some_bits(rc_arena *arena)
{
    static const uint32_t bits[] = {0, 5, 31, 32, 63, 64, 99};
    rc_bitset bs = rc_bitset_make(100, arena);

    for (uint32_t i = 0; i < RC_COUNT(bits); i++) {
        rc_bitset_set(&bs, bits[i]);
    }
    return bs;
}


RC_TEST_STEP(bitset_foreach, empty, fix)
{
    rc_bitset none = {0};
    rc_bitset clear = rc_bitset_make(100, &fix->a);

    rc_bitset_foreach(&none);
    rc_bitset_foreach(&clear);
    RC_CHECK(seen.num, ==, 0u);
}


RC_TEST_STEP(bitset_foreach, set_bits_in_order, fix)
{
    rc_bitset bs = some_bits(&fix->a);

    rc_bitset_foreach(&bs);
    RC_CHECK(seen.num, ==, 7u);
    RC_CHECK(seen.indices[0], ==, 0u);
    RC_CHECK(seen.indices[1], ==, 5u);
    RC_CHECK(seen.indices[2], ==, 31u);
    RC_CHECK(seen.indices[3], ==, 32u);
    RC_CHECK(seen.indices[4], ==, 63u);
    RC_CHECK(seen.indices[5], ==, 64u);
    RC_CHECK(seen.indices[6], ==, 99u);
}


RC_TEST_STEP(bitset_foreach, context, fix)
{
    rc_bitset bs = some_bits(&fix->a);
    visits v = {0};

    foreach_with_context(&bs, &v);
    RC_CHECK(v.num, ==, 7u);
    RC_CHECK(v.indices[0], ==, 0u);
    RC_CHECK(v.indices[6], ==, 99u);
    RC_CHECK(seen.num, ==, 0u);
}
