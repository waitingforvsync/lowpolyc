// test_hash_trie_foreach.c
//
// Tests for the hash trie foreach template.

#include "richc/test.h"

// The key is its own hash
#define RC_HASH_TRIE_KEY_TYPE uint64_t
#define RC_HASH_TRIE_VALUE_TYPE int32_t
#define RC_HASH_TRIE_HASH(key) (key)
#define RC_HASH_TRIE_NAME fe
#include "richc/hash_trie.template.h"

// Every key hashes to 0, so the walk has to go down a chain of blocks
#define RC_HASH_TRIE_KEY_TYPE uint64_t
#define RC_HASH_TRIE_VALUE_TYPE int32_t
#define RC_HASH_TRIE_HASH(key) 0
#define RC_HASH_TRIE_NAME fe_chain
#include "richc/hash_trie.template.h"

// Only 256 hashes at width 8, so short chains hang below a bushy top, and 3 bits a level do not
// divide 32
#define RC_HASH_TRIE_KEY_TYPE uint32_t
#define RC_HASH_TRIE_HASH(key) ((key) & 0xFF)
#define RC_HASH_TRIE_WIDTH 8
#define RC_HASH_TRIE_NAME fe_weak
#include "richc/hash_trie.template.h"

// What a walk saw: the order is unspecified, so counts and sums.
typedef struct visits {
    uint32_t count;
    uint64_t key_sum;
    int32_t value_sum;
} visits;


static void visit(visits *v, const rc_hash_trie_fe_pool *pool, uint32_t index)
{
    v->count++;
    v->key_sum += rc_hash_trie_fe_key_get(pool, index);
    v->value_sum += rc_hash_trie_fe_value_get(pool, index);
}


static void visit_chain(visits *v, const rc_hash_trie_fe_chain_pool *pool, uint32_t index)
{
    v->count++;
    v->key_sum += rc_hash_trie_fe_chain_key_get(pool, index);
    v->value_sum += rc_hash_trie_fe_chain_value_get(pool, index);
}

// A read-only walk with a context: rc_hash_trie_fe_foreach
#define RC_HASH_TRIE_FOREACH_TRIE rc_hash_trie_fe
#define RC_HASH_TRIE_FOREACH_CTX visits
#define RC_HASH_TRIE_FOREACH_CONST
#define RC_HASH_TRIE_FOREACH_FUNC(ctx, pool, index) visit(ctx, pool, index)
#include "richc/hash_trie_foreach.template.h"

// A mutable walk with a context: rc_hash_trie_fe_chain_foreach
#define RC_HASH_TRIE_FOREACH_TRIE rc_hash_trie_fe_chain
#define RC_HASH_TRIE_FOREACH_CTX visits
#define RC_HASH_TRIE_FOREACH_FUNC(ctx, pool, index) visit_chain(ctx, pool, index)
#include "richc/hash_trie_foreach.template.h"

static uint32_t visit_count;


static void count_one(const rc_hash_trie_fe_pool *pool, uint32_t index)
{
    (void)pool;
    (void)index;
    visit_count++;
}


static void count_weak(const rc_hash_trie_fe_weak_pool *pool, uint32_t index)
{
    (void)pool;
    (void)index;
    visit_count++;
}

// A set, no context
#define RC_HASH_TRIE_FOREACH_TRIE rc_hash_trie_fe_weak
#define RC_HASH_TRIE_FOREACH_CONST
#define RC_HASH_TRIE_FOREACH_FUNC(pool, index) count_weak(pool, index)
#include "richc/hash_trie_foreach.template.h"

// No context, under another name
#define RC_HASH_TRIE_FOREACH_TRIE rc_hash_trie_fe
#define RC_HASH_TRIE_FOREACH_FUNC(pool, index) count_one(pool, index)
#define RC_HASH_TRIE_FOREACH_NAME count_entries
#include "richc/hash_trie_foreach.template.h"


static void double_value(rc_hash_trie_fe_pool *pool, uint32_t index)
{
    rc_hash_trie_fe_value_set(pool, index, rc_hash_trie_fe_value_get(pool, index) * 2);
}

// A mutable walk that changes the values
#define RC_HASH_TRIE_FOREACH_TRIE rc_hash_trie_fe
#define RC_HASH_TRIE_FOREACH_FUNC(pool, index) double_value(pool, index)
#define RC_HASH_TRIE_FOREACH_NAME double_values
#include "richc/hash_trie_foreach.template.h"

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(hash_trie_foreach) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(hash_trie_foreach, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(hash_trie_foreach, fix)
{
    rc_arena_release(&fix->a);
}


// A trie in pool with the keys 1 to n, each valued ten times its key.
static rc_hash_trie_fe trie_with(uint64_t n, rc_hash_trie_fe_pool *pool, rc_arena *arena)
{
    rc_hash_trie_fe trie = {0};

    for (uint64_t k = 1; k <= n; k++) {
        rc_hash_trie_fe_add(&trie, pool, k, (int32_t)(k * 10), arena);
    }
    return trie;
}


RC_TEST(hash_trie_foreach, empty)
{
    rc_hash_trie_fe_pool pool = {0};
    visits v = {0};

    rc_hash_trie_fe_foreach((rc_hash_trie_fe) {0}, &pool, &v);
    RC_CHECK(v.count, ==, 0u);
}


RC_TEST_STEP(hash_trie_foreach, single, fix)
{
    rc_hash_trie_fe_pool pool = {0};
    rc_hash_trie_fe trie = trie_with(1, &pool, &fix->a);
    visits v = {0};

    rc_hash_trie_fe_foreach(trie, &pool, &v);
    RC_CHECK(v.count, ==, 1u);
    RC_CHECK(v.key_sum, ==, (uint64_t)1);
    RC_CHECK(v.value_sum, ==, 10);
}


RC_TEST_STEP(hash_trie_foreach, visits_each_once, fix)
{
    rc_hash_trie_fe_pool pool = {0};
    rc_hash_trie_fe trie = trie_with(100, &pool, &fix->a);
    visits v = {0};

    rc_hash_trie_fe_foreach(trie, &pool, &v);
    RC_CHECK(v.count, ==, 100u);
    RC_CHECK(v.key_sum, ==, (uint64_t)5050);
    RC_CHECK(v.value_sum, ==, 50500);
}


RC_TEST_STEP(hash_trie_foreach, deep_chain, fix)
{
    rc_hash_trie_fe_chain_pool pool = {0};
    rc_hash_trie_fe_chain trie = {0};
    visits v = {0};

    for (uint64_t k = 1; k <= 20; k++) {
        rc_hash_trie_fe_chain_add(&trie, &pool, k, (int32_t)k, &fix->a);
    }
    rc_hash_trie_fe_chain_foreach(trie, &pool, &v);
    RC_CHECK(v.count, ==, 20u);
    RC_CHECK(v.key_sum, ==, (uint64_t)210);
    RC_CHECK(v.value_sum, ==, 210);
}


RC_TEST_STEP(hash_trie_foreach, skips_removed, fix)
{
    rc_hash_trie_fe_pool pool = {0};
    rc_hash_trie_fe trie = trie_with(10, &pool, &fix->a);
    visits v = {0};

    for (uint64_t k = 2; k <= 10; k += 2) {
        rc_hash_trie_fe_remove(trie, &pool, k);
    }
    rc_hash_trie_fe_foreach(trie, &pool, &v);
    RC_CHECK(v.count, ==, 5u);
    RC_CHECK(v.key_sum, ==, (uint64_t)25);
    RC_CHECK(v.value_sum, ==, 250);
}


RC_TEST_STEP(hash_trie_foreach, no_context, fix)
{
    rc_hash_trie_fe_pool pool = {0};
    rc_hash_trie_fe trie = trie_with(7, &pool, &fix->a);

    visit_count = 0;
    count_entries(trie, &pool);
    RC_CHECK(visit_count, ==, 7u);
}


RC_TEST_STEP(hash_trie_foreach, changes_values, fix)
{
    rc_hash_trie_fe_pool pool = {0};
    rc_hash_trie_fe trie = trie_with(5, &pool, &fix->a);

    double_values(trie, &pool);
    for (uint64_t k = 1; k <= 5; k++) {
        uint32_t i = rc_hash_trie_fe_find(trie, &pool, k);

        RC_CHECK(rc_hash_trie_fe_value_get(&pool, i), ==, (int32_t)(k * 20));
    }
}


RC_TEST_STEP(hash_trie_foreach, long_chain, fix)
{
    // The walk keeps its own list of blocks, so a chain of thousands is fine
    rc_hash_trie_fe_chain_pool pool = {0};
    rc_hash_trie_fe_chain trie = {0};
    visits v = {0};

    for (uint64_t k = 1; k <= 2000; k++) {
        rc_hash_trie_fe_chain_add(&trie, &pool, k, 1, &fix->a);
    }
    rc_hash_trie_fe_chain_foreach(trie, &pool, &v);
    RC_CHECK(v.count, ==, 2000u);
    RC_CHECK(v.key_sum, ==, (uint64_t)2001000);
}


RC_TEST_STEP(hash_trie_foreach, weak_hash, fix)
{
    rc_hash_trie_fe_weak_pool pool = {0};
    rc_hash_trie_fe_weak set = {0};

    for (uint32_t k = 0; k < 3000; k++) {
        rc_hash_trie_fe_weak_add(&set, &pool, k, &fix->a);
    }
    for (uint32_t k = 0; k < 3000; k += 3) {
        rc_hash_trie_fe_weak_remove(set, &pool, k);
    }
    visit_count = 0;
    rc_hash_trie_fe_weak_foreach(set, &pool);
    RC_CHECK(visit_count, ==, 2000u);
}
