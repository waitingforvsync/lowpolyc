// test_upper_bound.c
//
// Tests for the upper bound template.

#include "richc/test.h"

#define RC_ARRAY_TYPE int32_t
#define RC_ARRAY_NAME i32
#include "richc/array.template.h"

// Records, searched by key
typedef struct kv {
    int32_t key;
    int32_t value;
} kv;

#define RC_ARRAY_TYPE kv
#include "richc/array.template.h"

// A context whose sign flips the order, so one search serves ascending and descending views
typedef struct sign_ctx {
    int32_t sign;
} sign_ctx;

// The default comparison, and lower_bound to bracket runs with
#define RC_UPPER_BOUND_TYPE int32_t
#define RC_UPPER_BOUND_NAME rc_upper_bound_i32
#define RC_UPPER_BOUND_VIEW rc_view_i32
#include "richc/algorithms/upper_bound.template.h"

#define RC_LOWER_BOUND_TYPE int32_t
#define RC_LOWER_BOUND_NAME rc_lower_bound_i32
#define RC_LOWER_BOUND_VIEW rc_view_i32
#include "richc/algorithms/lower_bound.template.h"

// A comparison of whole records, searched for by a record
#define RC_UPPER_BOUND_TYPE kv
#define RC_UPPER_BOUND_CMP(a, b) ((a).key < (b).key)
#include "richc/algorithms/upper_bound.template.h"

// The same records projected to their keys, searched for by a key
#define RC_UPPER_BOUND_TYPE kv
#define RC_UPPER_BOUND_NAME rc_upper_bound_kv_by_key
#define RC_UPPER_BOUND_KEY(e) ((e).key)
#define RC_UPPER_BOUND_KEY_TYPE int32_t
#include "richc/algorithms/upper_bound.template.h"

// A context in the comparison
#define RC_UPPER_BOUND_TYPE int32_t
#define RC_UPPER_BOUND_NAME rc_upper_bound_i32_signed
#define RC_UPPER_BOUND_VIEW rc_view_i32
#define RC_UPPER_BOUND_CTX sign_ctx
#define RC_UPPER_BOUND_CMP(c, a, b) ((c)->sign * (a) < (c)->sign * (b))
#include "richc/algorithms/upper_bound.template.h"

// A context in the projection, with the default comparison
#define RC_UPPER_BOUND_TYPE kv
#define RC_UPPER_BOUND_NAME rc_upper_bound_kv_signed
#define RC_UPPER_BOUND_CTX sign_ctx
#define RC_UPPER_BOUND_KEY(c, e) ((c)->sign * (e).key)
#define RC_UPPER_BOUND_KEY_TYPE int32_t
#include "richc/algorithms/upper_bound.template.h"

// A context that neither the projection nor the comparison uses
#define RC_UPPER_BOUND_TYPE int32_t
#define RC_UPPER_BOUND_NAME rc_upper_bound_i32_ctx
#define RC_UPPER_BOUND_VIEW rc_view_i32
#define RC_UPPER_BOUND_CTX sign_ctx
#include "richc/algorithms/upper_bound.template.h"


RC_TEST(upper_bound, empty)
{
    rc_view_i32 v = {0};

    RC_CHECK(rc_upper_bound_i32(v, 42), ==, 0u);
}


RC_TEST(upper_bound, single)
{
    static const int32_t a[] = {10};
    rc_view_i32 v = RC_VIEW(a);

    RC_CHECK(rc_upper_bound_i32(v, 5), ==, 0u);
    RC_CHECK(rc_upper_bound_i32(v, 10), ==, 1u);
    RC_CHECK(rc_upper_bound_i32(v, 11), ==, 1u);
}


RC_TEST(upper_bound, past_either_end)
{
    static const int32_t a[] = {10, 20, 30};
    rc_view_i32 v = RC_VIEW(a);

    RC_CHECK(rc_upper_bound_i32(v, 0), ==, 0u);
    RC_CHECK(rc_upper_bound_i32(v, 100), ==, 3u);
}


RC_TEST(upper_bound, general)
{
    static const int32_t a[] = {2, 4, 6, 8, 10, 12};
    rc_view_i32 v = RC_VIEW(a);

    // Present or missing, the first greater
    RC_CHECK(rc_upper_bound_i32(v, 1), ==, 0u);
    RC_CHECK(rc_upper_bound_i32(v, 2), ==, 1u);
    RC_CHECK(rc_upper_bound_i32(v, 5), ==, 2u);
    RC_CHECK(rc_upper_bound_i32(v, 6), ==, 3u);
    RC_CHECK(rc_upper_bound_i32(v, 12), ==, 6u);
    RC_CHECK(rc_upper_bound_i32(v, 13), ==, 6u);
}


RC_TEST(upper_bound, duplicates)
{
    // One past the last of a run
    static const int32_t a[] = {1, 3, 3, 3, 5, 5, 7};
    rc_view_i32 v = RC_VIEW(a);

    RC_CHECK(rc_upper_bound_i32(v, 3), ==, 4u);
    RC_CHECK(rc_upper_bound_i32(v, 5), ==, 6u);
    RC_CHECK(rc_upper_bound_i32(v, 4), ==, 4u);
}


RC_TEST(upper_bound, brackets_runs_with_lower_bound)
{
    static const int32_t a[] = {1, 3, 3, 3, 5, 5, 7};
    rc_view_i32 v = RC_VIEW(a);

    // The run of 3s, the run of 5s, and the empty run where 4 would go
    RC_CHECK(rc_upper_bound_i32(v, 3) - rc_lower_bound_i32(v, 3), ==, 3u);
    RC_CHECK(rc_upper_bound_i32(v, 5) - rc_lower_bound_i32(v, 5), ==, 2u);
    RC_CHECK(rc_upper_bound_i32(v, 4) - rc_lower_bound_i32(v, 4), ==, 0u);
}


RC_TEST(upper_bound, custom_comparison)
{
    static const kv a[] = {{1, 100}, {3, 300}, {5, 500}, {5, 501}, {9, 900}};
    rc_view_kv v = RC_VIEW(a);

    RC_CHECK(rc_upper_bound_kv(v, (kv) {.key = 5}), ==, 4u);
    RC_CHECK(rc_upper_bound_kv(v, (kv) {.key = 4}), ==, 2u);
    RC_CHECK(rc_upper_bound_kv(v, (kv) {.key = 0}), ==, 0u);
    RC_CHECK(rc_upper_bound_kv(v, (kv) {.key = 9}), ==, 5u);
}


RC_TEST(upper_bound, projection)
{
    static const kv a[] = {{1, 100}, {3, 300}, {5, 500}, {5, 501}, {9, 900}};
    rc_view_kv v = RC_VIEW(a);

    RC_CHECK(rc_upper_bound_kv_by_key(v, 5), ==, 4u);
    RC_CHECK(rc_upper_bound_kv_by_key(v, 4), ==, 2u);
    RC_CHECK(rc_upper_bound_kv_by_key(v, 9), ==, 5u);
}


RC_TEST(upper_bound, context_in_the_comparison)
{
    // Ascending, then ascending under the flipped order, so the first value < the key
    static const int32_t up[] = {10, 20, 30, 40, 50};
    static const int32_t down[] = {50, 40, 30, 20, 10};
    rc_view_i32 v_up = RC_VIEW(up);
    rc_view_i32 v_down = RC_VIEW(down);
    sign_ctx ascending = {.sign = 1};
    sign_ctx descending = {.sign = -1};

    RC_CHECK(rc_upper_bound_i32_signed(v_up, &ascending, 30), ==, 3u);
    RC_CHECK(rc_upper_bound_i32_signed(v_up, &ascending, 25), ==, 2u);
    RC_CHECK(rc_upper_bound_i32_signed(v_up, &ascending, 55), ==, 5u);
    RC_CHECK(rc_upper_bound_i32_signed(v_down, &descending, 30), ==, 3u);
    RC_CHECK(rc_upper_bound_i32_signed(v_down, &descending, 35), ==, 2u);
    RC_CHECK(rc_upper_bound_i32_signed(v_down, &descending, 5), ==, 5u);
}


RC_TEST(upper_bound, context_in_the_projection)
{
    // Keys descending, so negated they ascend; the key searched for is already negated
    static const kv a[] = {{9, 900}, {5, 500}, {5, 501}, {3, 300}, {1, 100}};
    rc_view_kv v = RC_VIEW(a);
    sign_ctx descending = {.sign = -1};

    RC_CHECK(rc_upper_bound_kv_signed(v, &descending, -5), ==, 3u);
    RC_CHECK(rc_upper_bound_kv_signed(v, &descending, -4), ==, 3u);
    RC_CHECK(rc_upper_bound_kv_signed(v, &descending, -1), ==, 5u);
}


RC_TEST(upper_bound, unused_context)
{
    static const int32_t a[] = {2, 4, 6, 8};
    rc_view_i32 v = RC_VIEW(a);
    sign_ctx ctx = {.sign = 0};

    RC_CHECK(rc_upper_bound_i32_ctx(v, &ctx, 5), ==, 2u);
    RC_CHECK(rc_upper_bound_i32_ctx(v, &ctx, 6), ==, 3u);
}
