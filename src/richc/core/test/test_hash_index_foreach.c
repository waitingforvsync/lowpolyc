// test_hash_index_foreach.c
//
// Tests for the hash index foreach template.

#include "richc/arena.h"
#include "richc/hash.h"
#include "richc/test.h"

// How many times a walk saw each index, which is all it can promise: the order is the table's
enum { MAX_INDEX = 64 };

typedef struct tally {
    uint32_t num;
    uint32_t times[MAX_INDEX];
} tally;

// Where the walk without a context counts what it sees
static tally seen;


// Count index into t.
static void count(tally *t, uint32_t index)
{
    RC_ASSERT(index < MAX_INDEX);
    t->times[index]++;
    t->num++;
}


#define RC_HASH_INDEX_FOREACH_FUNC(i) count(&seen, i)
#include "richc/algorithm/hash_index_foreach.template.h"

#define RC_HASH_INDEX_FOREACH_CTX tally
#define RC_HASH_INDEX_FOREACH_FUNC(t, i) count(t, i)
#define RC_HASH_INDEX_FOREACH_NAME foreach_with_context
#include "richc/algorithm/hash_index_foreach.template.h"

// Each test runs against a fresh default arena, with nothing seen yet
RC_TEST_GROUP_DATA(hash_index_foreach) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(hash_index_foreach, fix)
{
    fix->a = rc_arena_make_default();
    seen = (tally) {0};
}


RC_TEST_GROUP_DEINIT(hash_index_foreach, fix)
{
    rc_arena_release(&fix->a);
}


RC_TEST_STEP(hash_index_foreach, empty, fix)
{
    rc_hash_index none = {0};
    rc_hash_index unused = rc_hash_index_make(10, &fix->a);

    rc_hash_index_foreach(none);
    rc_hash_index_foreach(unused);
    RC_CHECK(seen.num, ==, 0u);
}


RC_TEST_STEP(hash_index_foreach, every_entry_once, fix)
{
    // Enough entries for the table to grow several times, then every third removed again
    rc_hash_index table = {0};
    bool once_each = true;

    for (uint32_t i = 0; i < MAX_INDEX; i++) {
        rc_hash_index_add(&table, rc_hash_u32(i), i, &fix->a);
    }

    for (uint32_t i = 0; i < MAX_INDEX; i += 3) {
        RC_CHECK_TRUE(rc_hash_index_remove(&table, rc_hash_u32(i), i));
    }

    rc_hash_index_foreach(table);
    RC_CHECK(seen.num, ==, table.num);
    for (uint32_t i = 0; i < MAX_INDEX; i++) {
        once_each = once_each && seen.times[i] == (i % 3 == 0 ? 0u : 1u);
    }
    RC_CHECK_TRUE(once_each);
}


RC_TEST_STEP(hash_index_foreach, colliding_hashes, fix)
{
    // Every entry under the same hash, so one long run of slots, and hash 0, which is stored as 1
    rc_hash_index table = {0};
    tally t = {0};

    for (uint32_t i = 0; i < 20; i++) {
        rc_hash_index_add(&table, 0, i, &fix->a);
    }

    foreach_with_context(table, &t);
    RC_CHECK(t.num, ==, 20u);
    RC_CHECK(t.times[0], ==, 1u);
    RC_CHECK(t.times[19], ==, 1u);
    RC_CHECK(seen.num, ==, 0u);
}


RC_TEST_STEP(hash_index_foreach, an_index_under_two_hashes, fix)
{
    // As in a multimap, an index entered twice comes up twice
    rc_hash_index table = {0};

    rc_hash_index_add(&table, 5, 7, &fix->a);
    rc_hash_index_add(&table, 6, 7, &fix->a);
    rc_hash_index_add(&table, 9, 3, &fix->a);

    rc_hash_index_foreach(table);
    RC_CHECK(seen.num, ==, 3u);
    RC_CHECK(seen.times[7], ==, 2u);
    RC_CHECK(seen.times[3], ==, 1u);
}
