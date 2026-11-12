// test_bitset.c
//
// Tests for rc_bitset.

#include "richc/bitset.h"

#include "richc/arena.h"
#include "richc/test.h"

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(bitset) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(bitset, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(bitset, fix)
{
    rc_arena_release(&fix->a);
}


// A bitset of num bits with the given bits set.
static rc_bitset bitset_with(uint32_t num, const uint32_t *bits, uint32_t count, rc_arena *arena)
{
    rc_bitset bs = rc_bitset_make(num, arena);

    for (uint32_t i = 0; i < count; i++) {
        rc_bitset_set(&bs, bits[i]);
    }
    return bs;
}


// ---- making and sizing ----

RC_TEST(bitset, zero_init)
{
    rc_bitset bs = {0};

    RC_CHECK(bs.num, ==, 0u);
    RC_CHECK(bs.cap, ==, 0u);
    RC_CHECK_TRUE(bs.data == NULL);
    RC_CHECK(rc_bitset_first_set(&bs), ==, RC_INDEX_NONE);
    RC_CHECK(rc_bitset_count_set(&bs), ==, 0u);

    // Nothing to clear, and no buffer to touch
    rc_bitset_clear_all(&bs);
    RC_CHECK(bs.num, ==, 0u);
}


RC_TEST_STEP(bitset, make, fix)
{
    rc_bitset bs = rc_bitset_make(100, &fix->a);

    RC_CHECK(bs.num, ==, 100u);
    RC_CHECK(bs.cap, ==, 128u);
    for (uint32_t i = 0; i < 100; i++) {
        RC_CHECK_FALSE(rc_bitset_is_set(&bs, i));
    }
    RC_CHECK(rc_bitset_count_set(&bs), ==, 0u);
}


RC_TEST_STEP(bitset, make_word_aligned, fix)
{
    rc_bitset bs = rc_bitset_make(64, &fix->a);

    RC_CHECK(bs.num, ==, 64u);
    RC_CHECK(bs.cap, ==, 64u);
}


RC_TEST_STEP(bitset, make_one_bit, fix)
{
    rc_bitset bs = rc_bitset_make(1, &fix->a);

    RC_CHECK(bs.cap, ==, 32u);
    rc_bitset_set(&bs, 0);
    RC_CHECK(rc_bitset_first_set(&bs), ==, 0u);
}


RC_TEST(bitset, make_empty)
{
    // Nothing is allocated, so no arena is needed
    rc_bitset bs = rc_bitset_make(0, NULL);

    RC_CHECK(bs.num, ==, 0u);
    RC_CHECK(bs.cap, ==, 0u);
    RC_CHECK_TRUE(bs.data == NULL);
}


RC_TEST_STEP(bitset, make_copy, fix)
{
    uint32_t bits[] = {3, 64, 99};
    rc_bitset a = bitset_with(100, bits, 3, &fix->a);
    rc_bitset b = rc_bitset_make_copy(&a, &fix->a);

    RC_CHECK_TRUE(rc_bitset_is_equal(&a, &b));
    RC_CHECK_TRUE(a.data != b.data);

    // The copy has memory of its own
    rc_bitset_clear(&b, 3);
    rc_bitset_set(&b, 50);
    RC_CHECK_TRUE(rc_bitset_is_set(&a, 3));
    RC_CHECK_FALSE(rc_bitset_is_set(&a, 50));
}


RC_TEST(bitset, make_copy_empty)
{
    rc_bitset empty = {0};
    rc_bitset copy = rc_bitset_make_copy(&empty, NULL);

    RC_CHECK(copy.num, ==, 0u);
    RC_CHECK_TRUE(copy.data == NULL);
}


RC_TEST_STEP(bitset, reserve_rounds_to_words, fix)
{
    rc_bitset bs = {0};

    rc_bitset_reserve(&bs, 1, &fix->a);
    RC_CHECK(bs.cap, ==, 32u);
    rc_bitset_reserve(&bs, 33, &fix->a);
    RC_CHECK(bs.cap, ==, 64u);
    rc_bitset_reserve(&bs, 64, &fix->a);
    RC_CHECK(bs.cap, ==, 64u);
    rc_bitset_reserve(&bs, 65, &fix->a);
    RC_CHECK(bs.cap, ==, 96u);
    RC_CHECK(bs.num, ==, 0u);
}


RC_TEST_STEP(bitset, reserve_smaller_needs_no_arena, fix)
{
    rc_bitset bs = {0};

    rc_bitset_reserve(&bs, 100, &fix->a);
    rc_bitset_reserve(&bs, 50, NULL);
    RC_CHECK(bs.cap, ==, 128u);
}


RC_TEST_STEP(bitset, resize_grow_keeps_bits, fix)
{
    uint32_t bits[] = {10, 39};
    rc_bitset bs = bitset_with(40, bits, 2, &fix->a);

    rc_bitset_resize(&bs, 200, &fix->a);
    RC_CHECK(bs.num, ==, 200u);
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 10));
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 39));
    RC_CHECK(rc_bitset_next_set(&bs, 40), ==, RC_INDEX_NONE);
}


RC_TEST_STEP(bitset, resize_shrink_clears_dropped_bits, fix)
{
    // 70 is in the word that 66 cuts through; 150 is in a word wholly past it
    uint32_t bits[] = {5, 50, 70, 150};
    rc_bitset bs = bitset_with(200, bits, 4, &fix->a);

    rc_bitset_resize(&bs, 66, NULL);
    RC_CHECK(bs.num, ==, 66u);
    RC_CHECK(rc_bitset_count_set(&bs), ==, 2u);

    // Growing back shows the dropped bits were cleared
    rc_bitset_resize(&bs, 200, &fix->a);
    RC_CHECK(rc_bitset_count_set(&bs), ==, 2u);
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 70));
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 150));
}


RC_TEST_STEP(bitset, resize_shrink_to_word_boundary, fix)
{
    uint32_t bits[] = {10, 64, 100};
    rc_bitset bs = bitset_with(128, bits, 3, &fix->a);

    rc_bitset_resize(&bs, 64, NULL);
    rc_bitset_resize(&bs, 128, NULL);
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 10));
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 64));
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 100));
}


RC_TEST_STEP(bitset, resize_to_zero, fix)
{
    uint32_t bits[] = {0, 31};
    rc_bitset bs = bitset_with(32, bits, 2, &fix->a);

    rc_bitset_resize(&bs, 0, NULL);
    RC_CHECK(bs.num, ==, 0u);
    rc_bitset_resize(&bs, 32, NULL);
    RC_CHECK(rc_bitset_count_set(&bs), ==, 0u);
}


RC_TEST_STEP(bitset, push, fix)
{
    rc_bitset bs = {0};

    RC_CHECK(rc_bitset_push(&bs, true, &fix->a), ==, 0u);
    RC_CHECK(rc_bitset_push(&bs, false, &fix->a), ==, 1u);
    RC_CHECK(rc_bitset_push(&bs, true, &fix->a), ==, 2u);
    RC_CHECK(bs.num, ==, 3u);
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 0));
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 1));
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 2));

    // The first growth goes straight to 64 bits
    RC_CHECK(bs.cap, ==, 64u);
}


RC_TEST_STEP(bitset, push_many, fix)
{
    // Odd bits set and even ones clear, across several words and growths
    rc_bitset bs = {0};

    for (uint32_t i = 0; i < 300; i++) {
        RC_CHECK(rc_bitset_push(&bs, (i & 1) != 0, &fix->a), ==, i);
    }
    RC_CHECK(bs.num, ==, 300u);
    for (uint32_t i = 0; i < 300; i++) {
        RC_CHECK(rc_bitset_is_set(&bs, i), ==, (i & 1) != 0);
    }
}


RC_TEST_STEP(bitset, push_n_zero, fix)
{
    rc_bitset bs = {0};

    rc_bitset_push(&bs, true, &fix->a);
    RC_CHECK(rc_bitset_push_n_zero(&bs, 70, &fix->a), ==, 1u);
    RC_CHECK(bs.num, ==, 71u);
    RC_CHECK(rc_bitset_next_set(&bs, 1), ==, RC_INDEX_NONE);
    rc_bitset_set(&bs, 70);
    RC_CHECK(rc_bitset_push(&bs, false, &fix->a), ==, 71u);
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 70));
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 71));
}


RC_TEST_STEP(bitset, push_n_zero_none, fix)
{
    // Pushing nothing still reports where it would have gone
    rc_bitset bs = {0};

    rc_bitset_push_n_zero(&bs, 5, &fix->a);
    RC_CHECK(rc_bitset_push_n_zero(&bs, 0, &fix->a), ==, 5u);
    RC_CHECK(bs.num, ==, 5u);
}


RC_TEST_STEP(bitset, clear_all, fix)
{
    uint32_t bits[] = {3, 99};
    rc_bitset bs = bitset_with(100, bits, 2, &fix->a);

    rc_bitset_clear_all(&bs);
    RC_CHECK(bs.num, ==, 100u);
    RC_CHECK(bs.cap, ==, 128u);
    RC_CHECK(rc_bitset_first_set(&bs), ==, RC_INDEX_NONE);
}


RC_TEST_STEP(bitset, release, fix)
{
    // As the arena's last allocation, the words go straight back
    rc_arena_mark before = rc_arena_save(&fix->a);
    rc_bitset bs = rc_bitset_make(100, &fix->a);

    rc_bitset_release(&bs, &fix->a);
    RC_CHECK(bs.num, ==, 0u);
    RC_CHECK(bs.cap, ==, 0u);
    RC_CHECK_TRUE(bs.data == NULL);
    RC_CHECK(rc_arena_save(&fix->a).top, ==, before.top);
}


RC_TEST(bitset, release_empty)
{
    rc_bitset bs = {0};

    rc_bitset_release(&bs, NULL);
    RC_CHECK_TRUE(bs.data == NULL);
}


// ---- single bits ----

RC_TEST_STEP(bitset, set_clear_is_set, fix)
{
    rc_bitset bs = rc_bitset_make(100, &fix->a);

    // Either side of the first word boundary, and the last bit
    rc_bitset_set(&bs, 0);
    rc_bitset_set(&bs, 31);
    rc_bitset_set(&bs, 32);
    rc_bitset_set(&bs, 99);
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 0));
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 31));
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 32));
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 99));
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 30));
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 33));

    rc_bitset_clear(&bs, 31);
    RC_CHECK_FALSE(rc_bitset_is_set(&bs, 31));
    RC_CHECK_TRUE(rc_bitset_is_set(&bs, 32));

    // Setting a set bit, or clearing a clear one, changes nothing
    rc_bitset_set(&bs, 0);
    rc_bitset_clear(&bs, 31);
    RC_CHECK(rc_bitset_count_set(&bs), ==, 3u);
}


// ---- searching ----

RC_TEST_STEP(bitset, get_first_set, fix)
{
    rc_bitset bs = rc_bitset_make(200, &fix->a);

    RC_CHECK(rc_bitset_first_set(&bs), ==, RC_INDEX_NONE);
    rc_bitset_set(&bs, 130);
    rc_bitset_set(&bs, 7);
    RC_CHECK(rc_bitset_first_set(&bs), ==, 7u);
    rc_bitset_clear(&bs, 7);
    RC_CHECK(rc_bitset_first_set(&bs), ==, 130u);
}


RC_TEST_STEP(bitset, get_next_set_visits_in_order, fix)
{
    // Either side of word boundaries, and the very last bit
    uint32_t bits[] = {0, 1, 31, 32, 63, 64, 100, 200, 255};
    rc_bitset bs = bitset_with(256, bits, 9, &fix->a);
    uint32_t visited = 0;

    for (uint32_t i = rc_bitset_first_set(&bs); i != RC_INDEX_NONE; i = rc_bitset_next_set(&bs, i + 1)) {
        RC_CHECK(i, ==, bits[visited]);
        visited++;
    }
    RC_CHECK(visited, ==, 9u);
}


RC_TEST_STEP(bitset, get_next_set_from_pos, fix)
{
    uint32_t bits[] = {5, 40};
    rc_bitset bs = bitset_with(64, bits, 2, &fix->a);

    // pos on a set bit finds that bit
    RC_CHECK(rc_bitset_next_set(&bs, 0), ==, 5u);
    RC_CHECK(rc_bitset_next_set(&bs, 5), ==, 5u);
    RC_CHECK(rc_bitset_next_set(&bs, 6), ==, 40u);
    RC_CHECK(rc_bitset_next_set(&bs, 40), ==, 40u);
    RC_CHECK(rc_bitset_next_set(&bs, 41), ==, RC_INDEX_NONE);
    RC_CHECK(rc_bitset_next_set(&bs, 64), ==, RC_INDEX_NONE);
    RC_CHECK(rc_bitset_next_set(&bs, RC_INDEX_NONE), ==, RC_INDEX_NONE);
}


// ---- whole sets ----

RC_TEST_STEP(bitset, copy, fix)
{
    uint32_t a_bits[] = {1, 39};
    uint32_t b_bits[] = {5};
    rc_bitset a = bitset_with(40, a_bits, 2, &fix->a);
    rc_bitset b = bitset_with(40, b_bits, 1, &fix->a);

    rc_bitset_copy(&b, &a);
    RC_CHECK_TRUE(rc_bitset_is_equal(&a, &b));
    RC_CHECK_FALSE(rc_bitset_is_set(&b, 5));
}


RC_TEST_STEP(bitset, union_same_size, fix)
{
    uint32_t a_bits[] = {1, 40};
    uint32_t b_bits[] = {40, 63};
    rc_bitset a = bitset_with(64, a_bits, 2, &fix->a);
    rc_bitset b = bitset_with(64, b_bits, 2, &fix->a);

    rc_bitset_union(&a, &b);
    RC_CHECK(rc_bitset_count_set(&a), ==, 3u);
    RC_CHECK_TRUE(rc_bitset_is_set(&a, 1));
    RC_CHECK_TRUE(rc_bitset_is_set(&a, 40));
    RC_CHECK_TRUE(rc_bitset_is_set(&a, 63));
}


RC_TEST_STEP(bitset, union_wider_src, fix)
{
    // 45 shares dst's last word but is past dst->num, so it must not leak into dst's tail, where
    // num_set_bits would count it
    uint32_t d_bits[] = {2};
    uint32_t s_bits[] = {39, 45, 70};
    rc_bitset d = bitset_with(40, d_bits, 1, &fix->a);
    rc_bitset s = bitset_with(80, s_bits, 3, &fix->a);

    rc_bitset_union(&d, &s);
    RC_CHECK(d.num, ==, 40u);
    RC_CHECK(rc_bitset_count_set(&d), ==, 2u);
    RC_CHECK_TRUE(rc_bitset_is_set(&d, 39));
}


RC_TEST_STEP(bitset, union_narrower_src, fix)
{
    uint32_t d_bits[] = {70};
    uint32_t s_bits[] = {5};
    rc_bitset d = bitset_with(80, d_bits, 1, &fix->a);
    rc_bitset s = bitset_with(32, s_bits, 1, &fix->a);

    rc_bitset_union(&d, &s);
    RC_CHECK(rc_bitset_count_set(&d), ==, 2u);
    RC_CHECK_TRUE(rc_bitset_is_set(&d, 5));
    RC_CHECK_TRUE(rc_bitset_is_set(&d, 70));
}


RC_TEST_STEP(bitset, intersection_same_size, fix)
{
    uint32_t a_bits[] = {1, 40, 63};
    uint32_t b_bits[] = {40, 63};
    rc_bitset a = bitset_with(64, a_bits, 3, &fix->a);
    rc_bitset b = bitset_with(64, b_bits, 2, &fix->a);

    rc_bitset_intersection(&a, &b);
    RC_CHECK(rc_bitset_count_set(&a), ==, 2u);
    RC_CHECK_FALSE(rc_bitset_is_set(&a, 1));
}


RC_TEST_STEP(bitset, intersection_narrower_src, fix)
{
    // src has no bit 70, so it counts as clear
    uint32_t d_bits[] = {5, 70};
    uint32_t s_bits[] = {5, 20};
    rc_bitset d = bitset_with(80, d_bits, 2, &fix->a);
    rc_bitset s = bitset_with(32, s_bits, 2, &fix->a);

    rc_bitset_intersection(&d, &s);
    RC_CHECK(rc_bitset_count_set(&d), ==, 1u);
    RC_CHECK_TRUE(rc_bitset_is_set(&d, 5));
}


RC_TEST_STEP(bitset, intersection_disjoint, fix)
{
    uint32_t p_bits[] = {1};
    uint32_t q_bits[] = {2};
    rc_bitset p = bitset_with(40, p_bits, 1, &fix->a);
    rc_bitset q = bitset_with(40, q_bits, 1, &fix->a);

    rc_bitset_intersection(&p, &q);
    RC_CHECK(rc_bitset_count_set(&p), ==, 0u);
}


RC_TEST_STEP(bitset, intersects, fix)
{
    uint32_t a_bits[] = {10};
    uint32_t b_bits[] = {50};
    rc_bitset a = bitset_with(64, a_bits, 1, &fix->a);
    rc_bitset b = bitset_with(64, b_bits, 1, &fix->a);

    RC_CHECK_FALSE(rc_bitset_intersects(&a, &b));
    rc_bitset_set(&b, 10);
    RC_CHECK_TRUE(rc_bitset_intersects(&a, &b));
}


RC_TEST_STEP(bitset, intersects_different_sizes, fix)
{
    // Only the bits both have count, whichever way round
    uint32_t w_bits[] = {40};
    uint32_t n_bits[] = {7};
    rc_bitset w = bitset_with(80, w_bits, 1, &fix->a);
    rc_bitset n = bitset_with(32, n_bits, 1, &fix->a);

    RC_CHECK_FALSE(rc_bitset_intersects(&w, &n));
    rc_bitset_set(&w, 7);
    RC_CHECK_TRUE(rc_bitset_intersects(&w, &n));
    RC_CHECK_TRUE(rc_bitset_intersects(&n, &w));
}


RC_TEST_STEP(bitset, is_equal, fix)
{
    uint32_t bits[] = {3, 64};
    rc_bitset a = bitset_with(100, bits, 2, &fix->a);
    rc_bitset b = bitset_with(100, bits, 2, &fix->a);

    RC_CHECK_TRUE(rc_bitset_is_equal(&a, &b));
    rc_bitset_set(&b, 65);
    RC_CHECK_FALSE(rc_bitset_is_equal(&a, &b));
}


RC_TEST_STEP(bitset, is_equal_different_sizes, fix)
{
    // The same bits set, but not the same size
    uint32_t bits[] = {1};
    rc_bitset c = bitset_with(40, bits, 1, &fix->a);
    rc_bitset d = bitset_with(64, bits, 1, &fix->a);
    rc_bitset e = {0};
    rc_bitset f = {0};

    RC_CHECK_FALSE(rc_bitset_is_equal(&c, &d));
    RC_CHECK_TRUE(rc_bitset_is_equal(&e, &f));
}


RC_TEST_STEP(bitset, num_set_bits, fix)
{
    // 99 is in the last word, whose bits past num stay clear
    uint32_t bits[] = {0, 31, 32, 99};
    rc_bitset a = bitset_with(100, bits, 4, &fix->a);

    RC_CHECK(rc_bitset_count_set(&a), ==, 4u);
    rc_bitset_clear(&a, 31);
    RC_CHECK(rc_bitset_count_set(&a), ==, 3u);
}
