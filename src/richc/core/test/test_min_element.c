// test_min_element.c
//
// Tests for the min element template.

#include "richc/array/i32.h"
#include "richc/test.h"

// Records compared by key, so equal keys can be told apart by value
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
#define RC_MIN_ELEMENT_TYPE int32_t
#define RC_MIN_ELEMENT_NAME rc_min_element_i32
#define RC_MIN_ELEMENT_VIEW rc_view_i32
#include "richc/algorithm/min_element.template.h"

// A comparison of whole records
#define RC_MIN_ELEMENT_TYPE kv
#define RC_MIN_ELEMENT_CMP(a, b) ((a).key < (b).key)
#include "richc/algorithm/min_element.template.h"

// The same records projected to their values, a second search of the same type
#define RC_MIN_ELEMENT_TYPE kv
#define RC_MIN_ELEMENT_NAME rc_min_element_kv_by_value
#define RC_MIN_ELEMENT_KEY(e) ((e).value)
#define RC_MIN_ELEMENT_KEY_TYPE int32_t
#include "richc/algorithm/min_element.template.h"

// A context in the comparison
#define RC_MIN_ELEMENT_TYPE int32_t
#define RC_MIN_ELEMENT_NAME rc_min_element_i32_signed
#define RC_MIN_ELEMENT_VIEW rc_view_i32
#define RC_MIN_ELEMENT_CTX sign_ctx
#define RC_MIN_ELEMENT_CMP(c, a, b) ((c)->sign * (a) < (c)->sign * (b))
#include "richc/algorithm/min_element.template.h"

// A context in the projection, which counts each call
#define RC_MIN_ELEMENT_TYPE kv
#define RC_MIN_ELEMENT_NAME rc_min_element_kv_counted
#define RC_MIN_ELEMENT_CTX sign_ctx
#define RC_MIN_ELEMENT_KEY(c, e) ((c)->projections++, (c)->sign * (e).key)
#define RC_MIN_ELEMENT_KEY_TYPE int32_t
#include "richc/algorithm/min_element.template.h"

// A context that neither the projection nor the comparison uses
#define RC_MIN_ELEMENT_TYPE int32_t
#define RC_MIN_ELEMENT_NAME rc_min_element_i32_ctx
#define RC_MIN_ELEMENT_VIEW rc_view_i32
#define RC_MIN_ELEMENT_CTX sign_ctx
#include "richc/algorithm/min_element.template.h"


RC_TEST(min_element, empty)
{
    rc_view_i32 v = {0};

    RC_CHECK(rc_min_element_i32(v), ==, RC_INDEX_NONE);
}


RC_TEST(min_element, single)
{
    static const int32_t a[] = {7};
    rc_view_i32 v = RC_VIEW(a);

    RC_CHECK(rc_min_element_i32(v), ==, 0u);
}


RC_TEST(min_element, at_either_end_or_between)
{
    static const int32_t first[] = {-3, 4, 9, 2};
    static const int32_t middle[] = {5, 8, -1, 6};
    static const int32_t last[] = {5, 8, 6, -9};
    rc_view_i32 v_first = RC_VIEW(first);
    rc_view_i32 v_middle = RC_VIEW(middle);
    rc_view_i32 v_last = RC_VIEW(last);

    RC_CHECK(rc_min_element_i32(v_first), ==, 0u);
    RC_CHECK(rc_min_element_i32(v_middle), ==, 2u);
    RC_CHECK(rc_min_element_i32(v_last), ==, 3u);
}


RC_TEST(min_element, leftmost_of_a_tie)
{
    static const kv a[] = {{4, 0}, {2, 1}, {3, 2}, {2, 3}, {2, 4}};
    rc_view_kv v = RC_VIEW(a);

    RC_CHECK(rc_min_element_kv(v), ==, 1u);
}


RC_TEST(min_element, projection)
{
    static const kv a[] = {{1, 40}, {2, 10}, {3, 30}, {4, 10}};
    rc_view_kv v = RC_VIEW(a);

    RC_CHECK(rc_min_element_kv_by_value(v), ==, 1u);
}


RC_TEST(min_element, context_in_the_comparison)
{
    static const int32_t a[] = {3, 9, -2, 9, 5};
    rc_view_i32 v = RC_VIEW(a);
    sign_ctx up = {.sign = 1};
    sign_ctx down = {.sign = -1};

    RC_CHECK(rc_min_element_i32_signed(v, &up), ==, 2u);
    RC_CHECK(rc_min_element_i32_signed(v, &down), ==, 1u);
}


RC_TEST(min_element, each_element_projected_once)
{
    static const kv a[] = {{3, 0}, {9, 1}, {-2, 2}, {9, 3}, {5, 4}, {-2, 5}};
    rc_view_kv v = RC_VIEW(a);
    sign_ctx down = {.sign = -1};

    RC_CHECK(rc_min_element_kv_counted(v, &down), ==, 1u);
    RC_CHECK(down.projections, ==, RC_COUNT(a));
}


RC_TEST(min_element, unused_context)
{
    static const int32_t a[] = {6, 4, 8, 4};
    rc_view_i32 v = RC_VIEW(a);
    sign_ctx ctx = {.sign = 0};

    RC_CHECK(rc_min_element_i32_ctx(v, &ctx), ==, 1u);
}
