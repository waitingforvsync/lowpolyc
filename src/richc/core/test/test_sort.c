// test_sort.c
//
// Tests for the sort template.

#include "richc/arena.h"
#include "richc/test.h"

#define RC_ARRAY_TYPE int32_t
#define RC_ARRAY_NAME i32
#include "richc/array.template.h"

// Records sorted by key, so equal keys can be told apart by value
typedef struct kv {
    int32_t key;
    int32_t value;
} kv;

#define RC_ARRAY_TYPE kv
#include "richc/array.template.h"

// A context whose sign flips the order, and which counts the projections made through it
typedef struct sign_ctx {
    int32_t sign;
    uint32_t projections;
} sign_ctx;

// The default comparison
#define RC_SORT_TYPE int32_t
#define RC_SORT_NAME rc_sort_i32
#define RC_SORT_SPAN rc_span_i32
#include "richc/algorithm/sort.template.h"

// A comparison of whole records
#define RC_SORT_TYPE kv
#define RC_SORT_CMP(a, b) ((a).key < (b).key)
#include "richc/algorithm/sort.template.h"

// The same records projected to their values, a second sort of the same type
#define RC_SORT_TYPE kv
#define RC_SORT_NAME rc_sort_kv_by_value
#define RC_SORT_KEY(e) ((e).value)
#define RC_SORT_KEY_TYPE int32_t
#include "richc/algorithm/sort.template.h"

// A context in the comparison
#define RC_SORT_TYPE int32_t
#define RC_SORT_NAME rc_sort_i32_signed
#define RC_SORT_SPAN rc_span_i32
#define RC_SORT_CTX sign_ctx
#define RC_SORT_CMP(c, a, b) ((c)->sign * (a) < (c)->sign * (b))
#include "richc/algorithm/sort.template.h"

// Cached, with a context in the projection, which counts each call
#define RC_SORT_TYPE kv
#define RC_SORT_NAME rc_sort_kv_cached
#define RC_SORT_CTX sign_ctx
#define RC_SORT_KEY(c, e) ((c)->projections++, (c)->sign * (e).key)
#define RC_SORT_KEY_TYPE int32_t
#define RC_SORT_CACHED
#include "richc/algorithm/sort.template.h"

// Cached, with no context or projection
#define RC_SORT_TYPE int32_t
#define RC_SORT_NAME rc_sort_i32_cached
#define RC_SORT_SPAN rc_span_i32
#define RC_SORT_CACHED
#include "richc/algorithm/sort.template.h"

// Enough elements for several quicksort levels above insertion sort's 16
enum { MANY = 1000 };

// The cached sorts take their scratch from a fresh default arena.
RC_TEST_GROUP_DATA(sort) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(sort, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(sort, fix)
{
    rc_arena_release(&fix->a);
}


// True if v holds 0 to v.num - 1 in order.
static bool is_identity(rc_view_i32 v)
{
    for (uint32_t i = 0; i < v.num; i++) {
        if (rc_view_i32_get(v, i) != (int32_t)i) {
            return false;
        }
    }
    return true;
}


// The value 0 to num - 1 at index i of a shuffle, stepping through them a prime at a time.
static int32_t shuffled(uint32_t i, uint32_t num)
{
    return (int32_t)((i * 7919u) % num);
}


// Fill span with 0 to span.num - 1, shuffled.
static void fill_shuffled(rc_span_i32 span)
{
    for (uint32_t i = 0; i < span.num; i++) {
        rc_span_i32_set(span, i, shuffled(i, span.num));
    }
}


// Fill span with records whose keys are 0 to span.num - 1, shuffled, each value ten times its key.
static void fill_shuffled_records(rc_span_kv span)
{
    for (uint32_t i = 0; i < span.num; i++) {
        int32_t key = shuffled(i, span.num);

        rc_span_kv_set(span, i, (kv) {.key = key, .value = key * 10});
    }
}


RC_TEST(sort, empty_and_single)
{
    int32_t one[] = {42};
    rc_span_i32 empty = {0};
    rc_span_i32 single = RC_SPAN(one);

    rc_sort_i32(empty);
    rc_sort_i32(single);
    RC_CHECK(one[0], ==, 42);
}


RC_TEST(sort, small)
{
    // Few enough for insertion sort alone
    int32_t a[] = {5, -2, 9, 0, 5, 3, -7, 1};
    rc_span_i32 span = RC_SPAN(a);

    rc_sort_i32(span);
    RC_CHECK(a[0], ==, -7);
    RC_CHECK(a[1], ==, -2);
    RC_CHECK(a[2], ==, 0);
    RC_CHECK(a[3], ==, 1);
    RC_CHECK(a[4], ==, 3);
    RC_CHECK(a[5], ==, 5);
    RC_CHECK(a[6], ==, 5);
    RC_CHECK(a[7], ==, 9);
}


RC_TEST(sort, shuffled)
{
    int32_t a[MANY] = {0};
    rc_span_i32 span = RC_SPAN(a);

    fill_shuffled(span);
    rc_sort_i32(span);
    RC_CHECK_TRUE(is_identity(span.view));
}


RC_TEST(sort, every_size_to_40)
{
    // Every size either side of insertion sort's limit, and the smallest partitions
    int32_t a[40] = {0};

    for (uint32_t n = 0; n <= 40; n++) {
        rc_span_i32 span = rc_span_i32_from_ptr(a, n);

        fill_shuffled(span);
        rc_sort_i32(span);
        RC_CHECK_TRUE(is_identity(span.view));
    }
}


RC_TEST(sort, already_sorted_and_reversed)
{
    int32_t a[MANY] = {0};
    rc_span_i32 span = RC_SPAN(a);

    for (uint32_t i = 0; i < MANY; i++) {
        a[i] = (int32_t)i;
    }
    rc_sort_i32(span);
    RC_CHECK_TRUE(is_identity(span.view));

    for (uint32_t i = 0; i < MANY; i++) {
        a[i] = (int32_t)(MANY - 1 - i);
    }
    rc_sort_i32(span);
    RC_CHECK_TRUE(is_identity(span.view));
}


RC_TEST(sort, duplicates)
{
    // 0 to 9, each 100 times, so each run lands in its own block of 100
    int32_t a[MANY] = {0};
    rc_span_i32 span = RC_SPAN(a);
    bool in_blocks = true;

    for (uint32_t i = 0; i < MANY; i++) {
        a[i] = shuffled(i, MANY) % 10;
    }
    rc_sort_i32(span);
    for (uint32_t i = 0; i < MANY; i++) {
        in_blocks = in_blocks && a[i] == (int32_t)(i / 100);
    }
    RC_CHECK_TRUE(in_blocks);
}


RC_TEST(sort, all_equal)
{
    int32_t a[MANY] = {0};
    rc_span_i32 span = RC_SPAN(a);

    rc_sort_i32(span);
    RC_CHECK(a[0], ==, 0);
    RC_CHECK(a[MANY - 1], ==, 0);
}


RC_TEST(sort, heapsort_fallback)
{
    // No quicksort levels left, so the whole span is heapsorted
    int32_t a[MANY] = {0};
    rc_span_i32 span = RC_SPAN(a);

    fill_shuffled(span);
    rc_sort_i32_intro_(span, 0);
    RC_CHECK_TRUE(is_identity(span.view));
}


RC_TEST(sort, custom_comparison)
{
    // Only the keys are ordered; the values show the records moved whole
    kv a[] = {{3, 30}, {1, 10}, {4, 40}, {1, 11}, {5, 50}, {9, 90}, {2, 20}, {6, 60}};
    rc_span_kv span = RC_SPAN(a);

    rc_sort_kv(span);
    for (uint32_t i = 0; i < RC_COUNT(a); i++) {
        RC_CHECK(a[i].value / 10, ==, a[i].key);
    }
    RC_CHECK(a[0].key, ==, 1);
    RC_CHECK(a[1].key, ==, 1);
    RC_CHECK(a[2].key, ==, 2);
    RC_CHECK(a[7].key, ==, 9);
}


RC_TEST(sort, projection)
{
    // Ordered by value, the keys run backwards
    kv a[MANY] = {{0}};
    rc_span_kv span = RC_SPAN(a);
    bool by_value = true;

    for (uint32_t i = 0; i < MANY; i++) {
        int32_t key = shuffled(i, MANY);

        a[i] = (kv) {.key = key, .value = MANY - 1 - key};
    }
    rc_sort_kv_by_value(span);
    for (uint32_t i = 0; i < MANY; i++) {
        by_value = by_value && a[i].value == (int32_t)i && a[i].key == (int32_t)(MANY - 1 - i);
    }
    RC_CHECK_TRUE(by_value);
}


RC_TEST(sort, context_in_the_comparison)
{
    int32_t a[MANY] = {0};
    rc_span_i32 span = RC_SPAN(a);
    sign_ctx down = {.sign = -1};
    bool descending = true;

    fill_shuffled(span);
    rc_sort_i32_signed(span, &down);
    for (uint32_t i = 0; i < MANY; i++) {
        descending = descending && a[i] == (int32_t)(MANY - 1 - i);
    }
    RC_CHECK_TRUE(descending);
}


RC_TEST_STEP(sort, cached_projects_each_element_once, fix)
{
    // Descending by key, with every record moved whole
    kv a[MANY] = {{0}};
    rc_span_kv span = RC_SPAN(a);
    sign_ctx down = {.sign = -1};
    bool descending = true;

    fill_shuffled_records(span);
    rc_sort_kv_cached(span, &down, fix->a);
    for (uint32_t i = 0; i < MANY; i++) {
        int32_t key = (int32_t)(MANY - 1 - i);

        descending = descending && a[i].key == key && a[i].value == key * 10;
    }
    RC_CHECK_TRUE(descending);
    RC_CHECK(down.projections, ==, (uint32_t)MANY);
}


RC_TEST_STEP(sort, cached_every_size_to_40, fix)
{
    int32_t a[40] = {0};

    for (uint32_t n = 0; n <= 40; n++) {
        rc_span_i32 span = rc_span_i32_from_ptr(a, n);

        fill_shuffled(span);
        rc_sort_i32_cached(span, fix->a);
        RC_CHECK_TRUE(is_identity(span.view));
    }
}


RC_TEST_STEP(sort, cached_duplicates, fix)
{
    int32_t a[MANY] = {0};
    rc_span_i32 span = RC_SPAN(a);
    bool in_blocks = true;

    for (uint32_t i = 0; i < MANY; i++) {
        a[i] = shuffled(i, MANY) % 10;
    }
    rc_sort_i32_cached(span, fix->a);
    for (uint32_t i = 0; i < MANY; i++) {
        in_blocks = in_blocks && a[i] == (int32_t)(i / 100);
    }
    RC_CHECK_TRUE(in_blocks);
}
