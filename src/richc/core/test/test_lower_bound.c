// test_lower_bound.c
//
// Tests for the lower bound template.

#include "richc/test.h"

#define RC_ARRAY_TYPE int32_t
#define RC_ARRAY_NAME i32
#include "richc/array.template.h"

// The default comparison
#define RC_LOWER_BOUND_TYPE int32_t
#define RC_LOWER_BOUND_NAME i32
#include "richc/lower_bound.template.h"

// Records sorted by key, searched by a whole record
typedef struct kv {
    int32_t key;
    int32_t value;
} kv;

#define RC_ARRAY_TYPE kv
#include "richc/array.template.h"

#define RC_LOWER_BOUND_TYPE kv
#define RC_LOWER_BOUND_CMP(a, b) ((a).key < (b).key)
#include "richc/lower_bound.template.h"

// The same records under another name, searched by the key alone
typedef kv kv_by_key;

#define RC_ARRAY_TYPE kv_by_key
#include "richc/array.template.h"

#define RC_LOWER_BOUND_TYPE kv_by_key
#define RC_LOWER_BOUND_KEY_TYPE int32_t
#define RC_LOWER_BOUND_CMP(a, b) ((a).key < (b))
#include "richc/lower_bound.template.h"

// A context whose sign flips the order, so one search serves ascending and descending views
typedef struct sign_ctx {
    int32_t sign;
} sign_ctx;

typedef int32_t i32_signed;

#define RC_ARRAY_TYPE i32_signed
#include "richc/array.template.h"

#define RC_LOWER_BOUND_TYPE i32_signed
#define RC_LOWER_BOUND_CTX sign_ctx
#define RC_LOWER_BOUND_CMP(c, a, b) ((c)->sign * (a) < (c)->sign * (b))
#include "richc/lower_bound.template.h"

// A context with the default comparison, which ignores it
typedef int32_t i32_ctx;

#define RC_ARRAY_TYPE i32_ctx
#include "richc/array.template.h"

#define RC_LOWER_BOUND_TYPE i32_ctx
#define RC_LOWER_BOUND_CTX sign_ctx
#include "richc/lower_bound.template.h"

RC_TEST(lower_bound, empty)
{
    rc_view_i32 v = {0};

    RC_CHECK(rc_lower_bound_i32(v, 42), ==, 0u);
}


RC_TEST(lower_bound, single)
{
    static const int32_t a[] = {10};
    rc_view_i32 v = RC_VIEW(a);

    RC_CHECK(rc_lower_bound_i32(v, 5), ==, 0u);
    RC_CHECK(rc_lower_bound_i32(v, 10), ==, 0u);
    RC_CHECK(rc_lower_bound_i32(v, 11), ==, 1u);
}


RC_TEST(lower_bound, past_either_end)
{
    static const int32_t a[] = {10, 20, 30};
    rc_view_i32 v = RC_VIEW(a);

    RC_CHECK(rc_lower_bound_i32(v, 0), ==, 0u);
    RC_CHECK(rc_lower_bound_i32(v, 100), ==, 3u);
}


RC_TEST(lower_bound, general)
{
    static const int32_t a[] = {2, 4, 6, 8, 10, 12};
    rc_view_i32 v = RC_VIEW(a);

    // Present gives its index; missing gives the first greater
    RC_CHECK(rc_lower_bound_i32(v, 1), ==, 0u);
    RC_CHECK(rc_lower_bound_i32(v, 2), ==, 0u);
    RC_CHECK(rc_lower_bound_i32(v, 5), ==, 2u);
    RC_CHECK(rc_lower_bound_i32(v, 6), ==, 2u);
    RC_CHECK(rc_lower_bound_i32(v, 7), ==, 3u);
    RC_CHECK(rc_lower_bound_i32(v, 12), ==, 5u);
    RC_CHECK(rc_lower_bound_i32(v, 13), ==, 6u);
}


RC_TEST(lower_bound, duplicates)
{
    // The first of a run
    static const int32_t a[] = {1, 3, 3, 3, 5, 5, 7};
    rc_view_i32 v = RC_VIEW(a);

    RC_CHECK(rc_lower_bound_i32(v, 3), ==, 1u);
    RC_CHECK(rc_lower_bound_i32(v, 5), ==, 4u);
    RC_CHECK(rc_lower_bound_i32(v, 4), ==, 4u);
}


RC_TEST(lower_bound, custom_comparison)
{
    static const kv a[] = {{1, 100}, {3, 300}, {5, 500}, {5, 501}, {9, 900}};
    rc_view_kv v = RC_VIEW(a);

    RC_CHECK(rc_lower_bound_kv(v, (kv) {.key = 5}), ==, 2u);
    RC_CHECK(rc_lower_bound_kv(v, (kv) {.key = 4}), ==, 2u);
    RC_CHECK(rc_lower_bound_kv(v, (kv) {.key = 0}), ==, 0u);
    RC_CHECK(rc_lower_bound_kv(v, (kv) {.key = 10}), ==, 5u);
}


RC_TEST(lower_bound, key_type)
{
    static const kv_by_key a[] = {{1, 100}, {3, 300}, {5, 500}, {5, 501}, {9, 900}};
    rc_view_kv_by_key v = RC_VIEW(a);

    RC_CHECK(rc_lower_bound_kv_by_key(v, 5), ==, 2u);
    RC_CHECK(rc_lower_bound_kv_by_key(v, 6), ==, 4u);
    RC_CHECK(rc_lower_bound_kv_by_key(v, 10), ==, 5u);
}


RC_TEST(lower_bound, context_ascending)
{
    static const i32_signed a[] = {10, 20, 30, 40, 50};
    rc_view_i32_signed v = RC_VIEW(a);
    sign_ctx ctx = {.sign = 1};

    RC_CHECK(rc_lower_bound_i32_signed(v, &ctx, 30), ==, 2u);
    RC_CHECK(rc_lower_bound_i32_signed(v, &ctx, 25), ==, 2u);
    RC_CHECK(rc_lower_bound_i32_signed(v, &ctx, 5), ==, 0u);
    RC_CHECK(rc_lower_bound_i32_signed(v, &ctx, 55), ==, 5u);
}


RC_TEST(lower_bound, context_descending)
{
    // Ascending under the flipped order, so the first value <= the key
    static const i32_signed a[] = {50, 40, 30, 20, 10};
    rc_view_i32_signed v = RC_VIEW(a);
    sign_ctx ctx = {.sign = -1};

    RC_CHECK(rc_lower_bound_i32_signed(v, &ctx, 30), ==, 2u);
    RC_CHECK(rc_lower_bound_i32_signed(v, &ctx, 35), ==, 2u);
    RC_CHECK(rc_lower_bound_i32_signed(v, &ctx, 55), ==, 0u);
    RC_CHECK(rc_lower_bound_i32_signed(v, &ctx, 5), ==, 5u);
}


RC_TEST(lower_bound, context_with_default_comparison)
{
    static const i32_ctx a[] = {2, 4, 6, 8};
    rc_view_i32_ctx v = RC_VIEW(a);
    sign_ctx ctx = {.sign = 0};

    RC_CHECK(rc_lower_bound_i32_ctx(v, &ctx, 5), ==, 2u);
    RC_CHECK(rc_lower_bound_i32_ctx(v, &ctx, 6), ==, 2u);
}
