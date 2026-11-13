// test_hash_index.c
//
// Tests for the hash index and its template.

#include "richc/hash.h"
#include "richc/test.h"

// A position, as in vertex coincidence. It compares with ==, so -0 and +0 match, and
// rc_hash_f32 hashes them alike.
typedef struct pt {
    float x;
    float y;
    float z;
} pt;

// A key and value kept together in one array
typedef struct entry {
    uint32_t key;
    int32_t value;
} entry;

#define RC_ARRAY_TYPE pt
#include "richc/array.template.h"

#define RC_ARRAY_TYPE uint32_t
#define RC_ARRAY_NAME u32
#include "richc/array.template.h"

#define RC_ARRAY_TYPE entry
#include "richc/array.template.h"


static uint32_t pt_hash(pt p)
{
    uint32_t h = rc_hash_f32(p.x);

    h = rc_hash_combine(h, rc_hash_f32(p.y));
    return rc_hash_combine(h, rc_hash_f32(p.z));
}


static bool pt_is_equal(pt a, pt b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

// Positions in a view of their own
#define RC_HASH_INDEX_KEY_TYPE pt
#define RC_HASH_INDEX_DATA_TYPE rc_view_pt
#define RC_HASH_INDEX_KEY_AT(data, index) rc_view_pt_get(data, index)
#define RC_HASH_INDEX_HASH(key) pt_hash(key)
#define RC_HASH_INDEX_EQUAL(a, b) pt_is_equal(a, b)
#include "richc/hash_index.template.h"

// Keys with a good hash
#define RC_HASH_INDEX_KEY_TYPE uint32_t
#define RC_HASH_INDEX_DATA_TYPE rc_view_u32
#define RC_HASH_INDEX_KEY_AT(data, index) rc_view_u32_get(data, index)
#define RC_HASH_INDEX_HASH(key) rc_hash_u32(key)
#define RC_HASH_INDEX_NAME u32
#include "richc/hash_index.template.h"

// Keys with only 8 hashes, so long runs that wrap round the end of the table
#define RC_HASH_INDEX_KEY_TYPE uint32_t
#define RC_HASH_INDEX_DATA_TYPE rc_view_u32
#define RC_HASH_INDEX_KEY_AT(data, index) rc_view_u32_get(data, index)
#define RC_HASH_INDEX_HASH(key) (((key) & 7u) * 0x20000001u)
#define RC_HASH_INDEX_NAME weak
#include "richc/hash_index.template.h"

// Keys inside {key, value} structs
#define RC_HASH_INDEX_KEY_TYPE uint32_t
#define RC_HASH_INDEX_DATA_TYPE rc_view_entry
#define RC_HASH_INDEX_KEY_AT(data, index) rc_view_entry_get(data, index).key
#define RC_HASH_INDEX_HASH(key) rc_hash_u32(key)
#define RC_HASH_INDEX_NAME entry
#include "richc/hash_index.template.h"

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(hash_index) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(hash_index, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(hash_index, fix)
{
    rc_arena_release(&fix->a);
}


// True if the probe for hash yields index.
static bool has_entry(rc_hash_index table, uint32_t hash, uint32_t index)
{
    rc_hash_index_probe probe = rc_hash_index_probe_make(table, hash);
    uint32_t i = rc_hash_index_probe_next(table, &probe);

    while (i != RC_INDEX_NONE && i != index) {
        i = rc_hash_index_probe_next(table, &probe);
    }
    return i == index;
}


// How many entries the probe for hash yields.
static uint32_t count_entries(rc_hash_index table, uint32_t hash)
{
    rc_hash_index_probe probe = rc_hash_index_probe_make(table, hash);
    uint32_t count = 0;

    while (rc_hash_index_probe_next(table, &probe) != RC_INDEX_NONE) {
        count++;
    }
    return count;
}


// The next number from a simple generator, for the randomised tests.
static uint32_t next_random(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state >> 8;
}

// ---- the core: making and sizing ----

RC_TEST(hash_index, zero_is_empty)
{
    // Lookups, removes, reset and release on {0} touch no memory and need no arena
    rc_hash_index table = {0};
    rc_hash_index_probe probe = rc_hash_index_probe_make(table, 42);

    RC_CHECK(rc_hash_index_probe_next(table, &probe), ==, RC_INDEX_NONE);
    RC_CHECK_FALSE(rc_hash_index_remove(&table, 42, 0));
    RC_CHECK_FALSE(rc_hash_index_move(&table, 42, 0, 1));
    rc_hash_index_reset(&table);
    rc_hash_index_release(&table, NULL);
    RC_CHECK(table.cap, ==, 0u);
    RC_CHECK(table.num, ==, 0u);
}


RC_TEST(hash_index, make_zero_allocates_nothing)
{
    rc_hash_index table = rc_hash_index_make(0, NULL);

    RC_CHECK(table.cap, ==, 0u);
    RC_CHECK_TRUE(table.slots_ == NULL);
}


RC_TEST_STEP(hash_index, make_sizes_for_three_quarters, fix)
{
    // 8 slots hold 6 entries, 16 hold 12
    RC_CHECK(rc_hash_index_make(1, &fix->a).cap, ==, 8u);
    RC_CHECK(rc_hash_index_make(6, &fix->a).cap, ==, 8u);
    RC_CHECK(rc_hash_index_make(7, &fix->a).cap, ==, 16u);
    RC_CHECK(rc_hash_index_make(12, &fix->a).cap, ==, 16u);
    RC_CHECK(rc_hash_index_make(13, &fix->a).cap, ==, 32u);
    RC_CHECK(rc_hash_index_make(1000, &fix->a).cap, ==, 2048u);
}


RC_TEST_STEP(hash_index, reserve_never_shrinks, fix)
{
    rc_hash_index table = rc_hash_index_make(100, &fix->a);

    rc_hash_index_reserve(&table, 5, &fix->a);
    RC_CHECK(table.cap, ==, 256u);
}


RC_TEST_STEP(hash_index, add_with_room_needs_no_arena, fix)
{
    rc_hash_index table = rc_hash_index_make(6, &fix->a);

    for (uint32_t i = 0; i < 6; i++) {
        rc_hash_index_add(&table, rc_hash_u32(i), i, NULL);
    }
    RC_CHECK(table.num, ==, 6u);
    RC_CHECK(table.cap, ==, 8u);
}

// ---- the core: probes ----

RC_TEST_STEP(hash_index, probe_yields_each_match_once, fix)
{
    // Two entries under 10 and one under 20: a multimap
    rc_hash_index table = {0};

    rc_hash_index_add(&table, 10, 0, &fix->a);
    rc_hash_index_add(&table, 20, 1, &fix->a);
    rc_hash_index_add(&table, 10, 2, &fix->a);

    rc_hash_index_probe probe = rc_hash_index_probe_make(table, 10);
    RC_CHECK(rc_hash_index_probe_next(table, &probe), ==, 0u);
    RC_CHECK(rc_hash_index_probe_next(table, &probe), ==, 2u);
    RC_CHECK(rc_hash_index_probe_next(table, &probe), ==, RC_INDEX_NONE);
    RC_CHECK(rc_hash_index_probe_next(table, &probe), ==, RC_INDEX_NONE);
    RC_CHECK(count_entries(table, 20), ==, 1u);
    RC_CHECK(count_entries(table, 30), ==, 0u);
}


RC_TEST_STEP(hash_index, hash_zero_is_stored_as_one, fix)
{
    rc_hash_index table = {0};

    rc_hash_index_add(&table, 0, 5, &fix->a);
    RC_CHECK_TRUE(has_entry(table, 0, 5));
    RC_CHECK_TRUE(has_entry(table, 1, 5));
    RC_CHECK_TRUE(rc_hash_index_remove(&table, 0, 5));
    RC_CHECK(table.num, ==, 0u);
}

// ---- the core: growing ----

RC_TEST_STEP(hash_index, grows_and_keeps_entries, fix)
{
    rc_hash_index table = {0};

    for (uint32_t i = 0; i < 1000; i++) {
        rc_hash_index_add(&table, rc_hash_u32(i), i, &fix->a);
    }
    RC_CHECK(table.num, ==, 1000u);
    RC_CHECK(table.cap, ==, 2048u);

    uint32_t found = 0;
    for (uint32_t i = 0; i < 1000; i++) {
        found += has_entry(table, rc_hash_u32(i), i);
    }
    RC_CHECK(found, ==, 1000u);
}


RC_TEST_STEP(hash_index, grows_in_place_when_last, fix)
{
    rc_hash_index table = rc_hash_index_make(6, &fix->a);
    const rc_hash_index_slot_ *before = table.slots_;

    for (uint32_t i = 0; i < 7; i++) {
        rc_hash_index_add(&table, rc_hash_u32(i), i, &fix->a);
    }
    RC_CHECK(table.cap, ==, 16u);
    RC_CHECK_TRUE(table.slots_ == before);
}


RC_TEST_STEP(hash_index, grows_after_other_allocations, fix)
{
    // Something allocated since the table, so growth moves it
    rc_hash_index table = rc_hash_index_make(6, &fix->a);
    const rc_hash_index_slot_ *before = table.slots_;

    rc_arena_alloc(&fix->a, 100);
    for (uint32_t i = 0; i < 7; i++) {
        rc_hash_index_add(&table, rc_hash_u32(i), i, &fix->a);
    }
    RC_CHECK_TRUE(table.slots_ != before);

    uint32_t found = 0;
    for (uint32_t i = 0; i < 7; i++) {
        found += has_entry(table, rc_hash_u32(i), i);
    }
    RC_CHECK(found, ==, 7u);
}


RC_TEST_STEP(hash_index, growing_rehashes_wrapped_runs, fix)
{
    // In 8 slots, the hashes all have home 7, so the run wraps to slots 0 to 3. In 16, four have
    // home 15 and wrap again, and 7 stays at home.
    static const uint32_t hashes[] = {15, 31, 47, 63, 7};
    rc_hash_index table = rc_hash_index_make(6, &fix->a);

    for (uint32_t i = 0; i < 5; i++) {
        rc_hash_index_add(&table, hashes[i], i, &fix->a);
    }
    rc_hash_index_reserve(&table, 7, &fix->a);
    RC_CHECK(table.cap, ==, 16u);
    for (uint32_t i = 0; i < 5; i++) {
        RC_CHECK_TRUE(has_entry(table, hashes[i], i));
    }
    RC_CHECK(table.num, ==, 5u);
}


RC_TEST_STEP(hash_index, growing_by_a_lot_at_once, fix)
{
    // 8 slots to 2048 in one step, with runs crowding the end
    rc_hash_index table = rc_hash_index_make(6, &fix->a);

    for (uint32_t i = 0; i < 6; i++) {
        rc_hash_index_add(&table, 7u + 8u * i * 37u, i, &fix->a);
    }
    rc_hash_index_reserve(&table, 1000, &fix->a);
    RC_CHECK(table.cap, ==, 2048u);
    for (uint32_t i = 0; i < 6; i++) {
        RC_CHECK_TRUE(has_entry(table, 7u + 8u * i * 37u, i));
    }
}


RC_TEST_STEP(hash_index, same_hash_throughout, fix)
{
    // Every entry under one hash: a single run, through several growths
    rc_hash_index table = {0};

    for (uint32_t i = 0; i < 300; i++) {
        rc_hash_index_add(&table, 0xDEADBEEFu, i, &fix->a);
    }
    RC_CHECK(count_entries(table, 0xDEADBEEFu), ==, 300u);
    for (uint32_t i = 0; i < 300; i += 2) {
        RC_CHECK_TRUE(rc_hash_index_remove(&table, 0xDEADBEEFu, i));
    }
    RC_CHECK(count_entries(table, 0xDEADBEEFu), ==, 150u);
    RC_CHECK_TRUE(has_entry(table, 0xDEADBEEFu, 299));
    RC_CHECK_FALSE(has_entry(table, 0xDEADBEEFu, 298));
}

// ---- the core: removing, moving and resetting ----

RC_TEST_STEP(hash_index, remove_closes_wrapped_runs, fix)
{
    // All home 6 in 8 slots: a run over 6, 7, 0 and 1. Removing from its middle moves the rest
    // back, and every other entry stays reachable.
    rc_hash_index table = rc_hash_index_make(6, &fix->a);

    for (uint32_t i = 0; i < 4; i++) {
        rc_hash_index_add(&table, 6u + 8u * i, i, &fix->a);
    }
    RC_CHECK_TRUE(rc_hash_index_remove(&table, 14, 1));
    RC_CHECK(table.num, ==, 3u);
    RC_CHECK_TRUE(has_entry(table, 6, 0));
    RC_CHECK_FALSE(has_entry(table, 14, 1));
    RC_CHECK_TRUE(has_entry(table, 22, 2));
    RC_CHECK_TRUE(has_entry(table, 30, 3));

    RC_CHECK_TRUE(rc_hash_index_remove(&table, 6, 0));
    RC_CHECK_TRUE(has_entry(table, 22, 2));
    RC_CHECK_TRUE(has_entry(table, 30, 3));
}


RC_TEST_STEP(hash_index, remove_leaves_later_homes_alone, fix)
{
    // Homes 1, 1, 2 and 5 fill slots 1, 2, 3 and 5. Removing the first moves the second to 1 and
    // the third to 2, but slot 5's entry is past the run's end.
    rc_hash_index table = rc_hash_index_make(6, &fix->a);

    rc_hash_index_add(&table, 1, 0, &fix->a);
    rc_hash_index_add(&table, 9, 1, &fix->a);
    rc_hash_index_add(&table, 2, 2, &fix->a);
    rc_hash_index_add(&table, 5, 3, &fix->a);
    RC_CHECK_TRUE(rc_hash_index_remove(&table, 1, 0));
    RC_CHECK(table.slots_[1].index, ==, 1u);
    RC_CHECK(table.slots_[2].index, ==, 2u);
    RC_CHECK(table.slots_[3].hash, ==, 0u);
    RC_CHECK(table.slots_[5].index, ==, 3u);
}


RC_TEST_STEP(hash_index, remove_needs_hash_and_index, fix)
{
    rc_hash_index table = {0};

    rc_hash_index_add(&table, 10, 3, &fix->a);
    RC_CHECK_FALSE(rc_hash_index_remove(&table, 10, 4));
    RC_CHECK_FALSE(rc_hash_index_remove(&table, 11, 3));
    RC_CHECK(table.num, ==, 1u);
    RC_CHECK_TRUE(rc_hash_index_remove(&table, 10, 3));
    RC_CHECK_FALSE(rc_hash_index_remove(&table, 10, 3));
}


RC_TEST_STEP(hash_index, move_repoints_one_entry, fix)
{
    rc_hash_index table = {0};

    rc_hash_index_add(&table, 9, 3, &fix->a);
    rc_hash_index_add(&table, 9, 4, &fix->a);
    RC_CHECK_TRUE(rc_hash_index_move(&table, 9, 3, 7));
    RC_CHECK_TRUE(has_entry(table, 9, 7));
    RC_CHECK_TRUE(has_entry(table, 9, 4));
    RC_CHECK_FALSE(has_entry(table, 9, 3));
    RC_CHECK_FALSE(rc_hash_index_move(&table, 9, 3, 8));
    RC_CHECK(table.num, ==, 2u);
}


RC_TEST_STEP(hash_index, reset_keeps_slots, fix)
{
    rc_hash_index table = {0};

    for (uint32_t i = 0; i < 20; i++) {
        rc_hash_index_add(&table, rc_hash_u32(i), i, &fix->a);
    }
    rc_hash_index_reset(&table);
    RC_CHECK(table.num, ==, 0u);
    RC_CHECK(table.cap, ==, 32u);
    RC_CHECK_FALSE(has_entry(table, rc_hash_u32(3), 3));

    rc_hash_index_add(&table, 5, 0, NULL);
    RC_CHECK_TRUE(has_entry(table, 5, 0));
}


RC_TEST_STEP(hash_index, release_gives_back_last_allocation, fix)
{
    rc_hash_index table = rc_hash_index_make(100, &fix->a);
    void *slots = table.slots_;

    rc_hash_index_release(&table, &fix->a);
    RC_CHECK(table.cap, ==, 0u);
    RC_CHECK_TRUE(table.slots_ == NULL);
    RC_CHECK_TRUE(rc_arena_alloc(&fix->a, 16) == slots);
}

// ---- the template ----

RC_TEST(hash_index, typed_lookups_on_empty)
{
    rc_hash_index table = {0};
    rc_view_u32 keys = {0};

    RC_CHECK(rc_hash_index_u32_find(table, keys, 1), ==, RC_INDEX_NONE);
    RC_CHECK_FALSE(rc_hash_index_u32_contains(table, keys, 1));
    RC_CHECK(rc_hash_index_u32_remove(&table, keys, 1), ==, RC_INDEX_NONE);
}


RC_TEST_STEP(hash_index, find_or_add_gathers_unique_positions, fix)
{
    // Six corners on four positions, -0 matching +0, with room for every corner up front
    static const pt corners[] = {
        {0, 0, 0},
        {1, 0, 0},
        {-0.0f, 0, 0},
        {0, 1, 0},
        {1, 0, 0},
        {1, 1, 0}
    };
    static const uint32_t expected[] = {0, 1, 0, 2, 1, 3};
    rc_array_pt unique = rc_array_pt_make(6, &fix->a);
    rc_hash_index table = rc_hash_index_make(6, &fix->a);

    for (uint32_t c = 0; c < 6; c++) {
        uint32_t i = rc_hash_index_pt_find_or_add(&table, unique.view, corners[c], unique.num, NULL);

        if (i == unique.num) {
            rc_array_pt_push(&unique, corners[c], NULL);
        }
        RC_CHECK(i, ==, expected[c]);
    }
    RC_CHECK(unique.num, ==, 4u);
    RC_CHECK(table.num, ==, 4u);
    RC_CHECK(rc_hash_index_pt_find(table, unique.view, (pt) {1, 1, 0}), ==, 3u);
    RC_CHECK(rc_hash_index_pt_find(table, unique.view, (pt) {2, 0, 0}), ==, RC_INDEX_NONE);
}


RC_TEST_STEP(hash_index, add_indexes_existing_data, fix)
{
    // Indexing an array after the fact, duplicates and all
    static uint32_t keys_data[] = {5, 9, 5, 12};
    rc_view_u32 keys = RC_VIEW(keys_data);
    rc_hash_index table = {0};

    for (uint32_t i = 0; i < keys.num; i++) {
        rc_hash_index_u32_add(&table, keys, i, &fix->a);
    }
    RC_CHECK(table.num, ==, 4u);
    RC_CHECK(rc_hash_index_u32_find(table, keys, 9), ==, 1u);
    RC_CHECK(rc_hash_index_u32_find(table, keys, 12), ==, 3u);
    RC_CHECK(count_entries(table, rc_hash_u32(5)), ==, 2u);

    // Each remove takes one of the two entries for 5
    uint32_t first = rc_hash_index_u32_remove(&table, keys, 5);
    uint32_t second = rc_hash_index_u32_remove(&table, keys, 5);
    RC_CHECK(first + second, ==, 2u);
    RC_CHECK(rc_hash_index_u32_remove(&table, keys, 5), ==, RC_INDEX_NONE);
    RC_CHECK(table.num, ==, 2u);
}


RC_TEST_STEP(hash_index, keys_inside_structs, fix)
{
    static entry entries_data[] = {{3, 30}, {7, 70}, {11, 110}};
    rc_view_entry entries = RC_VIEW(entries_data);
    rc_hash_index table = {0};

    for (uint32_t i = 0; i < entries.num; i++) {
        rc_hash_index_entry_add(&table, entries, i, &fix->a);
    }

    uint32_t i = rc_hash_index_entry_find(table, entries, 7);
    RC_CHECK(rc_view_entry_get(entries, i).value, ==, 70);
    RC_CHECK_FALSE(rc_hash_index_entry_contains(table, entries, 4));
}


RC_TEST_STEP(hash_index, swap_remove_keeps_data_dense, fix)
{
    rc_array_u32 keys = {0};
    rc_hash_index table = {0};

    for (uint32_t k = 0; k < 50; k++) {
        rc_hash_index_u32_find_or_add(&table, keys.view, k * 3, keys.num, &fix->a);
        rc_array_u32_push(&keys, k * 3, &fix->a);
    }

    // Remove every key divisible by 4, moving the last key into each gap
    for (uint32_t k = 0; k < 50; k += 4) {
        uint32_t i = rc_hash_index_u32_remove(&table, keys.view, k * 3);
        uint32_t last = keys.num - 1;

        RC_CHECK(i, !=, RC_INDEX_NONE);
        if (i != last) {
            RC_CHECK_TRUE(rc_hash_index_u32_move(&table, keys.view, last, i));
            rc_array_u32_set(&keys, i, rc_array_u32_get(&keys, last));
        }
        rc_array_u32_pop(&keys);
    }
    RC_CHECK(keys.num, ==, 37u);
    RC_CHECK(table.num, ==, 37u);

    // Every key left is found where it now lives, and the removed ones are gone
    uint32_t right = 0;
    for (uint32_t i = 0; i < keys.num; i++) {
        right += rc_hash_index_u32_find(table, keys.view, rc_array_u32_get(&keys, i)) == i;
    }
    RC_CHECK(right, ==, 37u);
    RC_CHECK_FALSE(rc_hash_index_u32_contains(table, keys.view, 0));
    RC_CHECK_FALSE(rc_hash_index_u32_contains(table, keys.view, 12));
}

// ---- randomised, against a plain search ----

// Random adds and swap-removes of keys below 200 with a weak hash, checking after each change
// that every key below 200 is found exactly where a plain search of the keys finds it.
RC_TEST_STEP(hash_index, random_changes_match_a_plain_search, fix)
{
    rc_array_u32 keys = {0};
    rc_hash_index table = {0};
    uint32_t state = 12345;
    uint32_t mismatches = 0;

    for (uint32_t step = 0; step < 4000; step++) {
        uint32_t key = next_random(&state) % 200;

        // Mostly adds early on, mostly removes later, so the table grows and drains
        if (next_random(&state) % 4000 >= step) {
            uint32_t i = rc_hash_index_weak_find_or_add(&table, keys.view, key, keys.num, &fix->a);

            if (i == keys.num) {
                rc_array_u32_push(&keys, key, &fix->a);
            }
        }
        else {
            uint32_t i = rc_hash_index_weak_remove(&table, keys.view, key);

            if (i != RC_INDEX_NONE) {
                uint32_t last = keys.num - 1;

                if (i != last) {
                    rc_hash_index_weak_move(&table, keys.view, last, i);
                    rc_array_u32_set(&keys, i, rc_array_u32_get(&keys, last));
                }
                rc_array_u32_pop(&keys);
            }
        }

        // Where a plain search finds each key
        uint32_t where[200];
        for (uint32_t k = 0; k < 200; k++) {
            where[k] = RC_INDEX_NONE;
        }
        for (uint32_t i = 0; i < keys.num; i++) {
            where[rc_array_u32_get(&keys, i)] = i;
        }

        mismatches += table.num != keys.num;
        for (uint32_t k = 0; k < 200; k++) {
            mismatches += rc_hash_index_weak_find(table, keys.view, k) != where[k];
        }
    }
    RC_CHECK(mismatches, ==, 0u);
}
