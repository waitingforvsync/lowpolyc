// test_hash_trie.c
//
// Tests for the hash trie template.

#include "richc/hash.h"
#include "richc/test.h"

// The key is its own hash, so tests can make keys collide exactly. At width 4, each level takes
// the next 2 bits.
#define RC_HASH_TRIE_KEY_TYPE uint64_t
#define RC_HASH_TRIE_VALUE_TYPE int32_t
#define RC_HASH_TRIE_HASH(key) (key)
#define RC_HASH_TRIE_NAME test
#include "richc/hash_trie.template.h"

// Every key hashes to 0, so the keys form one deep chain
#define RC_HASH_TRIE_KEY_TYPE uint64_t
#define RC_HASH_TRIE_VALUE_TYPE int32_t
#define RC_HASH_TRIE_HASH(key) 0
#define RC_HASH_TRIE_NAME collide
#include "richc/hash_trie.template.h"

// A set: no value type
#define RC_HASH_TRIE_KEY_TYPE uint64_t
#define RC_HASH_TRIE_HASH(key) (key)
#define RC_HASH_TRIE_NAME set
#include "richc/hash_trie.template.h"

// The other widths, with a real hash
#define RC_HASH_TRIE_KEY_TYPE uint32_t
#define RC_HASH_TRIE_VALUE_TYPE uint32_t
#define RC_HASH_TRIE_HASH(key) rc_hash_u32(key)
#define RC_HASH_TRIE_WIDTH 2
#define RC_HASH_TRIE_NAME w2
#include "richc/hash_trie.template.h"

#define RC_HASH_TRIE_KEY_TYPE uint32_t
#define RC_HASH_TRIE_VALUE_TYPE uint32_t
#define RC_HASH_TRIE_HASH(key) rc_hash_u32(key)
#define RC_HASH_TRIE_WIDTH 8
#define RC_HASH_TRIE_NAME w8
#include "richc/hash_trie.template.h"

#define RC_HASH_TRIE_KEY_TYPE uint32_t
#define RC_HASH_TRIE_VALUE_TYPE uint32_t
#define RC_HASH_TRIE_HASH(key) rc_hash_u32(key)
#define RC_HASH_TRIE_WIDTH 16
#define RC_HASH_TRIE_NAME w16
#include "richc/hash_trie.template.h"

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(hash_trie) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(hash_trie, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(hash_trie, fix)
{
    rc_arena_release(&fix->a);
}


// key's value, or -1 if it is not there.
static int32_t value_of(rc_hash_trie_test trie, const rc_hash_trie_test_pool *pool, uint64_t key)
{
    uint32_t index = rc_hash_trie_test_find(trie, pool, key);

    return index == RC_INDEX_NONE ? -1 : rc_hash_trie_test_value_get(pool, index);
}


static int32_t collide_value_of(rc_hash_trie_collide trie, const rc_hash_trie_collide_pool *pool, uint64_t key)
{
    uint32_t index = rc_hash_trie_collide_find(trie, pool, key);

    return index == RC_INDEX_NONE ? -1 : rc_hash_trie_collide_value_get(pool, index);
}

// ---- empty tries ----

RC_TEST(hash_trie, zero_is_empty)
{
    // Reads, removes and release on {0} touch no memory
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test trie = {0};

    RC_CHECK(rc_hash_trie_test_find(trie, &pool, 42), ==, RC_INDEX_NONE);
    RC_CHECK_FALSE(rc_hash_trie_test_contains(trie, &pool, 42));
    RC_CHECK_FALSE(rc_hash_trie_test_remove(trie, &pool, 42));
    rc_hash_trie_test_release(&trie, &pool);
    RC_CHECK(trie.root_, ==, 0u);
}


RC_TEST_STEP(hash_trie, first_add_makes_root, fix)
{
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test trie = {0};

    uint32_t i = rc_hash_trie_test_add(&trie, &pool, 42, 420, &fix->a);
    RC_CHECK(trie.root_, ==, 1u);
    RC_CHECK(rc_hash_trie_test_value_get(&pool, i), ==, 420);
}


RC_TEST_STEP(hash_trie, copies_share_contents, fix)
{
    // A trie is only its root's index, so a copy sees the same entries
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test trie = {0};

    rc_hash_trie_test_add(&trie, &pool, 1, 11, &fix->a);
    rc_hash_trie_test_add(&trie, &pool, 2, 22, &fix->a);

    rc_hash_trie_test copy = trie;
    RC_CHECK(value_of(copy, &pool, 1), ==, 11);
    RC_CHECK(value_of(copy, &pool, 2), ==, 22);

    rc_hash_trie_test_add(&trie, &pool, 3, 33, &fix->a);
    RC_CHECK(value_of(copy, &pool, 3), ==, 33);
}

// ---- add, set, find and contains ----

RC_TEST_STEP(hash_trie, add_find, fix)
{
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test trie = {0};

    uint32_t i = rc_hash_trie_test_add(&trie, &pool, 10, 100, &fix->a);
    uint32_t j = rc_hash_trie_test_add(&trie, &pool, 20, 200, &fix->a);
    RC_CHECK(i, !=, j);
    RC_CHECK(rc_hash_trie_test_find(trie, &pool, 10), ==, i);
    RC_CHECK(rc_hash_trie_test_find(trie, &pool, 20), ==, j);
    RC_CHECK(rc_hash_trie_test_value_get(&pool, i), ==, 100);
    RC_CHECK(rc_hash_trie_test_value_get(&pool, j), ==, 200);
    RC_CHECK(rc_hash_trie_test_find(trie, &pool, 30), ==, RC_INDEX_NONE);
}


RC_TEST_STEP(hash_trie, add_existing_keeps_value, fix)
{
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test trie = {0};

    uint32_t i = rc_hash_trie_test_add(&trie, &pool, 10, 100, &fix->a);
    RC_CHECK(rc_hash_trie_test_add(&trie, &pool, 10, 999, &fix->a), ==, i);
    RC_CHECK(value_of(trie, &pool, 10), ==, 100);
}


RC_TEST_STEP(hash_trie, set_adds_or_replaces, fix)
{
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test trie = {0};

    uint32_t i = rc_hash_trie_test_set(&trie, &pool, 10, 100, &fix->a);
    RC_CHECK(value_of(trie, &pool, 10), ==, 100);
    RC_CHECK(rc_hash_trie_test_set(&trie, &pool, 10, 999, &fix->a), ==, i);
    RC_CHECK(value_of(trie, &pool, 10), ==, 999);
}


RC_TEST_STEP(hash_trie, contains, fix)
{
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test trie = {0};

    rc_hash_trie_test_add(&trie, &pool, 10, 100, &fix->a);
    RC_CHECK_TRUE(rc_hash_trie_test_contains(trie, &pool, 10));
    RC_CHECK_FALSE(rc_hash_trie_test_contains(trie, &pool, 20));
    rc_hash_trie_test_remove(trie, &pool, 10);
    RC_CHECK_FALSE(rc_hash_trie_test_contains(trie, &pool, 10));
}


RC_TEST_STEP(hash_trie, collisions, fix)
{
    // 0x10, 0x20 and 0x30 share their low 4 bits, the first two levels' slots. 0x10 takes the root
    // slot, 0x20 the slot below it, and 0x30 the slot below that.
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test trie = {0};

    uint32_t i = rc_hash_trie_test_add(&trie, &pool, 0x10, 1, &fix->a);
    uint32_t j = rc_hash_trie_test_add(&trie, &pool, 0x20, 2, &fix->a);
    uint32_t k = rc_hash_trie_test_add(&trie, &pool, 0x30, 3, &fix->a);
    RC_CHECK(i / 4, ==, 0u);
    RC_CHECK(j / 4, !=, i / 4);
    RC_CHECK(k / 4, !=, j / 4);
    RC_CHECK(k / 4, !=, i / 4);
    RC_CHECK(value_of(trie, &pool, 0x10), ==, 1);
    RC_CHECK(value_of(trie, &pool, 0x20), ==, 2);
    RC_CHECK(value_of(trie, &pool, 0x30), ==, 3);
}

// ---- access by node index ----

RC_TEST_STEP(hash_trie, value_access, fix)
{
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test trie = {0};

    uint32_t i = rc_hash_trie_test_add(&trie, &pool, 5, 50, &fix->a);
    rc_hash_trie_test_value_set(&pool, i, 51);
    RC_CHECK(rc_hash_trie_test_value_get(&pool, i), ==, 51);
    *rc_hash_trie_test_value_at(&pool, i) = 52;
    RC_CHECK(value_of(trie, &pool, 5), ==, 52);

    // A colliding key makes child blocks, growing the pool; the index still holds
    rc_hash_trie_test_add(&trie, &pool, 0x15, 150, &fix->a);
    RC_CHECK(rc_hash_trie_test_value_get(&pool, i), ==, 52);
    RC_CHECK(value_of(trie, &pool, 0x15), ==, 150);
}


RC_TEST_STEP(hash_trie, key_access, fix)
{
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test trie = {0};

    uint32_t i = rc_hash_trie_test_add(&trie, &pool, 10, 100, &fix->a);
    uint32_t j = rc_hash_trie_test_add(&trie, &pool, 0x1A, 150, &fix->a);
    RC_CHECK(rc_hash_trie_test_key_get(&pool, i), ==, (uint64_t)10);
    RC_CHECK(rc_hash_trie_test_key_get(&pool, j), ==, (uint64_t)0x1A);

    // The colliding key is in another block
    RC_CHECK(j / 4, !=, i / 4);
}

// ---- sets ----

RC_TEST_STEP(hash_trie, set, fix)
{
    rc_hash_trie_set_pool pool = {0};
    rc_hash_trie_set set = {0};

    RC_CHECK_FALSE(rc_hash_trie_set_contains(set, &pool, 0x10));

    uint32_t i = rc_hash_trie_set_add(&set, &pool, 0x10, &fix->a);
    rc_hash_trie_set_add(&set, &pool, 0x20, &fix->a);
    rc_hash_trie_set_add(&set, &pool, 0x30, &fix->a);
    RC_CHECK(rc_hash_trie_set_add(&set, &pool, 0x10, &fix->a), ==, i);
    RC_CHECK(rc_hash_trie_set_find(set, &pool, 0x10), ==, i);
    RC_CHECK(rc_hash_trie_set_key_get(&pool, i), ==, (uint64_t)0x10);

    RC_CHECK_TRUE(rc_hash_trie_set_contains(set, &pool, 0x10));
    RC_CHECK_TRUE(rc_hash_trie_set_contains(set, &pool, 0x20));
    RC_CHECK_TRUE(rc_hash_trie_set_contains(set, &pool, 0x30));
    RC_CHECK_FALSE(rc_hash_trie_set_contains(set, &pool, 0x40));

    RC_CHECK_TRUE(rc_hash_trie_set_remove(set, &pool, 0x20));
    RC_CHECK_FALSE(rc_hash_trie_set_contains(set, &pool, 0x20));
    RC_CHECK_TRUE(rc_hash_trie_set_contains(set, &pool, 0x10));
    RC_CHECK_TRUE(rc_hash_trie_set_contains(set, &pool, 0x30));
    RC_CHECK_FALSE(rc_hash_trie_set_remove(set, &pool, 0x20));
}

// ---- remove ----

RC_TEST_STEP(hash_trie, remove_leaf, fix)
{
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test trie = {0};

    rc_hash_trie_test_add(&trie, &pool, 10, 100, &fix->a);
    rc_hash_trie_test_add(&trie, &pool, 20, 200, &fix->a);
    RC_CHECK_TRUE(rc_hash_trie_test_remove(trie, &pool, 10));
    RC_CHECK(rc_hash_trie_test_find(trie, &pool, 10), ==, RC_INDEX_NONE);
    RC_CHECK(value_of(trie, &pool, 20), ==, 200);
    RC_CHECK_FALSE(rc_hash_trie_test_remove(trie, &pool, 10));
}


RC_TEST_STEP(hash_trie, remove_with_children, fix)
{
    // 0x10 holds the root slot with 0x20 and 0x30 below it, so 0x20, the first below, moves up
    // into it
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test trie = {0};

    uint32_t i = rc_hash_trie_test_add(&trie, &pool, 0x10, 1, &fix->a);
    rc_hash_trie_test_add(&trie, &pool, 0x20, 2, &fix->a);
    rc_hash_trie_test_add(&trie, &pool, 0x30, 3, &fix->a);
    RC_CHECK_TRUE(rc_hash_trie_test_remove(trie, &pool, 0x10));
    RC_CHECK(rc_hash_trie_test_find(trie, &pool, 0x10), ==, RC_INDEX_NONE);
    RC_CHECK(rc_hash_trie_test_find(trie, &pool, 0x20), ==, i);
    RC_CHECK(value_of(trie, &pool, 0x20), ==, 2);
    RC_CHECK(value_of(trie, &pool, 0x30), ==, 3);
}


RC_TEST_STEP(hash_trie, remove_all_then_add, fix)
{
    // Emptying the child blocks unlinks them, and the trie still works
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test trie = {0};

    rc_hash_trie_test_add(&trie, &pool, 0x10, 1, &fix->a);
    rc_hash_trie_test_add(&trie, &pool, 0x20, 2, &fix->a);
    RC_CHECK_TRUE(rc_hash_trie_test_remove(trie, &pool, 0x20));
    RC_CHECK_TRUE(rc_hash_trie_test_remove(trie, &pool, 0x10));
    RC_CHECK(rc_hash_trie_test_find(trie, &pool, 0x10), ==, RC_INDEX_NONE);
    RC_CHECK(rc_hash_trie_test_find(trie, &pool, 0x20), ==, RC_INDEX_NONE);

    rc_hash_trie_test_add(&trie, &pool, 0x10, 5, &fix->a);
    RC_CHECK(value_of(trie, &pool, 0x10), ==, 5);
}


RC_TEST_STEP(hash_trie, many, fix)
{
    // 200 keys make several blocks and move the pool; remove the even ones
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test trie = {0};

    for (uint64_t k = 1; k <= 200; k++) {
        rc_hash_trie_test_add(&trie, &pool, k, (int32_t)(k * 2), &fix->a);
    }
    for (uint64_t k = 1; k <= 200; k++) {
        RC_CHECK(value_of(trie, &pool, k), ==, (int32_t)(k * 2));
    }
    for (uint64_t k = 2; k <= 200; k += 2) {
        RC_CHECK_TRUE(rc_hash_trie_test_remove(trie, &pool, k));
    }
    for (uint64_t k = 1; k <= 200; k++) {
        RC_CHECK(value_of(trie, &pool, k), ==, k % 2 == 0 ? -1 : (int32_t)(k * 2));
    }
}

// ---- pools and release ----

RC_TEST_STEP(hash_trie, shared_pool, fix)
{
    // Two tries on one pool keep their own entries
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test a = {0};
    rc_hash_trie_test b = {0};

    rc_hash_trie_test_add(&a, &pool, 42, 1, &fix->a);
    rc_hash_trie_test_add(&b, &pool, 42, 2, &fix->a);
    RC_CHECK(value_of(a, &pool, 42), ==, 1);
    RC_CHECK(value_of(b, &pool, 42), ==, 2);

    rc_hash_trie_test_add(&a, &pool, 99, 9, &fix->a);
    RC_CHECK(rc_hash_trie_test_find(b, &pool, 99), ==, RC_INDEX_NONE);
}


RC_TEST_STEP(hash_trie, pool_lifecycle, fix)
{
    rc_hash_trie_test_pool pool = rc_hash_trie_test_pool_make(4, &fix->a);
    rc_hash_trie_test trie = {0};

    RC_CHECK(pool.blocks_.slots_.cap, ==, 4u);
    rc_hash_trie_test_pool_reserve(&pool, 32, &fix->a);
    RC_CHECK(pool.blocks_.slots_.cap, ==, 32u);

    rc_hash_trie_test_add(&trie, &pool, 7, 70, &fix->a);
    RC_CHECK(value_of(trie, &pool, 7), ==, 70);

    rc_hash_trie_test_pool_release(&pool, &fix->a);
    RC_CHECK(pool.blocks_.slots_.num, ==, 0u);
    RC_CHECK(pool.blocks_.slots_.cap, ==, 0u);
    RC_CHECK_TRUE(pool.blocks_.slots_.data == NULL);
}


RC_TEST_STEP(hash_trie, release_frees_every_block, fix)
{
    // Another trie of the same keys fits in the released blocks, without growing the pool
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test a = {0};
    rc_hash_trie_test b = {0};

    for (uint64_t k = 1; k <= 200; k++) {
        rc_hash_trie_test_add(&a, &pool, k, (int32_t)k, &fix->a);
    }

    uint32_t blocks = pool.blocks_.slots_.num;
    rc_hash_trie_test_release(&a, &pool);
    RC_CHECK(a.root_, ==, 0u);
    for (uint64_t k = 1; k <= 200; k++) {
        rc_hash_trie_test_add(&b, &pool, k, (int32_t)k, &fix->a);
    }
    RC_CHECK(pool.blocks_.slots_.num, ==, blocks);
    RC_CHECK(value_of(b, &pool, 123), ==, 123);
}


RC_TEST_STEP(hash_trie, released_root_becomes_a_child, fix)
{
    // a's root is block 0. Once a is released, block 0 is the next child block b makes.
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test a = {0};
    rc_hash_trie_test b = {0};

    rc_hash_trie_test_add(&a, &pool, 1, 1, &fix->a);
    rc_hash_trie_test_add(&b, &pool, 0x10, 16, &fix->a);
    rc_hash_trie_test_release(&a, &pool);

    uint32_t i = rc_hash_trie_test_add(&b, &pool, 0x20, 32, &fix->a);
    RC_CHECK(i / 4, ==, 0u);
    RC_CHECK(pool.blocks_.slots_.num, ==, 2u);
    RC_CHECK(value_of(b, &pool, 0x10), ==, 16);
    RC_CHECK(value_of(b, &pool, 0x20), ==, 32);
    RC_CHECK_TRUE(rc_hash_trie_test_remove(b, &pool, 0x10));
    RC_CHECK(value_of(b, &pool, 0x20), ==, 32);
}


RC_TEST_STEP(hash_trie, release_leaves_other_tries, fix)
{
    rc_hash_trie_test_pool pool = {0};
    rc_hash_trie_test a = {0};
    rc_hash_trie_test b = {0};

    for (uint64_t k = 1; k <= 50; k++) {
        rc_hash_trie_test_add(&a, &pool, k, 1, &fix->a);
        rc_hash_trie_test_add(&b, &pool, k, 2, &fix->a);
    }
    rc_hash_trie_test_release(&a, &pool);
    for (uint64_t k = 1; k <= 50; k++) {
        RC_CHECK(value_of(b, &pool, k), ==, 2);
    }
}

// ---- hash collisions ----

RC_TEST_STEP(hash_trie, deep_chain, fix)
{
    rc_hash_trie_collide_pool pool = {0};
    rc_hash_trie_collide trie = {0};

    for (uint64_t k = 1; k <= 20; k++) {
        rc_hash_trie_collide_add(&trie, &pool, k, (int32_t)k, &fix->a);
    }
    for (uint64_t k = 1; k <= 20; k++) {
        RC_CHECK(collide_value_of(trie, &pool, k), ==, (int32_t)k);
    }

    // Removing from the middle of the chain leaves the rest
    RC_CHECK_TRUE(rc_hash_trie_collide_remove(trie, &pool, 10));
    RC_CHECK(rc_hash_trie_collide_find(trie, &pool, 10), ==, RC_INDEX_NONE);
    for (uint64_t k = 1; k <= 20; k++) {
        if (k != 10) {
            RC_CHECK(collide_value_of(trie, &pool, k), ==, (int32_t)k);
        }
    }
}


RC_TEST_STEP(hash_trie, long_chain, fix)
{
    // Long enough that recursing a level per key would risk a small stack
    rc_hash_trie_collide_pool pool = {0};
    rc_hash_trie_collide trie = {0};

    for (uint64_t k = 1; k <= 2000; k++) {
        rc_hash_trie_collide_add(&trie, &pool, k, (int32_t)k, &fix->a);
    }
    RC_CHECK_TRUE(rc_hash_trie_collide_remove(trie, &pool, 1));
    RC_CHECK_TRUE(rc_hash_trie_collide_remove(trie, &pool, 2000));
    RC_CHECK(collide_value_of(trie, &pool, 1000), ==, 1000);

    uint32_t blocks = pool.blocks_.slots_.num;
    rc_hash_trie_collide_release(&trie, &pool);
    for (uint64_t k = 1; k <= 1998; k++) {
        rc_hash_trie_collide_add(&trie, &pool, k, (int32_t)k, &fix->a);
    }
    RC_CHECK(pool.blocks_.slots_.num, ==, blocks);
}


RC_TEST_STEP(hash_trie, emptied_blocks_are_reused, fix)
{
    // A chain of n keys takes about n blocks; removing and re-adding them reuses those blocks
    rc_hash_trie_collide_pool pool = {0};
    rc_hash_trie_collide trie = {0};

    for (uint64_t k = 1; k <= 30; k++) {
        rc_hash_trie_collide_add(&trie, &pool, k, (int32_t)k, &fix->a);
    }

    uint32_t blocks = pool.blocks_.slots_.num;
    for (uint32_t cycle = 0; cycle < 5; cycle++) {
        for (uint64_t k = 1; k <= 30; k++) {
            RC_CHECK_TRUE(rc_hash_trie_collide_remove(trie, &pool, k));
        }
        for (uint64_t k = 1; k <= 30; k++) {
            rc_hash_trie_collide_add(&trie, &pool, k, (int32_t)k, &fix->a);
        }
        RC_CHECK(pool.blocks_.slots_.num, <=, blocks);
        RC_CHECK(collide_value_of(trie, &pool, 15), ==, 15);
    }
}

// ---- other widths ----

// Add 1000 keys, remove the odd ones, check the rest, and release
#define WIDTH_TEST(NAME)                                                                          \
    RC_TEST_STEP(hash_trie, width_##NAME, fix)                                                    \
    {                                                                                             \
        rc_hash_trie_##NAME##_pool pool = {0};                                                    \
        rc_hash_trie_##NAME trie = {0};                                                           \
                                                                                                  \
        for (uint32_t k = 0; k < 1000; k++) {                                                     \
            rc_hash_trie_##NAME##_add(&trie, &pool, k, k * 3, &fix->a);                           \
        }                                                                                         \
        for (uint32_t k = 1; k < 1000; k += 2) {                                                  \
            RC_CHECK_TRUE(rc_hash_trie_##NAME##_remove(trie, &pool, k));                          \
        }                                                                                         \
        for (uint32_t k = 0; k < 1000; k++) {                                                     \
            uint32_t i = rc_hash_trie_##NAME##_find(trie, &pool, k);                              \
                                                                                                  \
            if (k % 2 == 0) {                                                                     \
                RC_CHECK(rc_hash_trie_##NAME##_value_get(&pool, i), ==, k * 3);                   \
            }                                                                                     \
            else {                                                                                \
                RC_CHECK(i, ==, RC_INDEX_NONE);                                                   \
            }                                                                                     \
        }                                                                                         \
        rc_hash_trie_##NAME##_release(&trie, &pool);                                              \
        RC_CHECK(trie.root_, ==, 0u);                                                             \
    }

WIDTH_TEST(w2)
WIDTH_TEST(w8)
WIDTH_TEST(w16)
