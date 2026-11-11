// test_array.c
//
// Tests for the view, span and array template.

#include "richc/test.h"

#define RC_ARRAY_TYPE int32_t
#define RC_ARRAY_NAME i32
#include "richc/array.template.h"

// A struct element, so rotate is seen to move whole elements
typedef struct pt {
    int32_t x;
    int32_t y;
} pt;

// With no RC_ARRAY_NAME, the type's own name is the suffix
#define RC_ARRAY_TYPE pt
#include "richc/array.template.h"

// A type holding a view of itself, which needs the two-pass instantiation
typedef struct node node;
#define RC_ARRAY_TYPE node
#define RC_ARRAY_DECLARE_ONLY
#include "richc/array.template.h"

struct node {
    rc_view_node children;
    int32_t id;
};

#define RC_ARRAY_TYPE node
#define RC_ARRAY_IMPL_ONLY
#include "richc/array.template.h"

// Most tests run against a fresh default arena.
RC_TEST_GROUP_DATA(array) {
    rc_arena a;
};


RC_TEST_GROUP_INIT(array, fix)
{
    fix->a = rc_arena_make_default();
}


RC_TEST_GROUP_DEINIT(array, fix)
{
    rc_arena_release(&fix->a);
}


// Push 0, 1, ..., n - 1.
static void push_count(rc_array_i32 *a, uint32_t n, rc_arena *arena)
{
    for (uint32_t i = 0; i < n; i++) {
        rc_array_i32_push(a, (int32_t)i, arena);
    }
}


// True when v holds exactly the n values in expect.
static bool view_is(rc_view_i32 v, const int32_t *expect, uint32_t n)
{
    if (v.num != n) {
        return false;
    }
    for (uint32_t i = 0; i < n; i++) {
        if (rc_view_i32_get(v, i) != expect[i]) {
            return false;
        }
    }
    return true;
}


// ---- construction ----

RC_TEST_STEP(array, make, fix)
{
    rc_array_i32 a = rc_array_i32_make(16, &fix->a);

    RC_CHECK(a.num, ==, 0u);
    RC_CHECK(a.cap, ==, 16u);
    RC_CHECK_TRUE(rc_array_i32_is_valid(&a));
}


RC_TEST(array, make_empty)
{
    // Zero capacity allocates nothing, so no arena is needed
    rc_array_i32 a = rc_array_i32_make(0, NULL);

    RC_CHECK(a.num, ==, 0u);
    RC_CHECK(a.cap, ==, 0u);
    RC_CHECK_FALSE(rc_array_i32_is_valid(&a));
}


RC_TEST_STEP(array, from_view, fix)
{
    int32_t src[] = {1, 2, 3, 4};
    rc_view_i32 v = RC_VIEW(src);
    rc_array_i32 a = rc_array_i32_from_view(v, 10, &fix->a);

    RC_CHECK_TRUE(view_is(a.view, src, 4));
    RC_CHECK(a.cap, ==, 10u);
    RC_CHECK_TRUE(a.data != src);
}


RC_TEST_STEP(array, from_view_capacity_fits_view, fix)
{
    // A minimum below the view's size gives exactly the view's size
    int32_t src[] = {1, 2, 3, 4};
    rc_view_i32 v = RC_VIEW(src);
    rc_array_i32 a = rc_array_i32_from_view(v, 2, &fix->a);

    RC_CHECK(a.cap, ==, 4u);
    RC_CHECK_TRUE(view_is(a.view, src, 4));
}


RC_TEST(array, from_empty_view)
{
    rc_array_i32 a = rc_array_i32_from_view((rc_view_i32) {0}, 0, NULL);

    RC_CHECK(a.num, ==, 0u);
    RC_CHECK_FALSE(rc_array_i32_is_valid(&a));
}


// ---- element access ----

RC_TEST_STEP(array, get_set_at, fix)
{
    rc_array_i32 a = {0};

    rc_array_i32_push(&a, 10, &fix->a);
    rc_array_i32_push(&a, 20, &fix->a);
    RC_CHECK(rc_array_i32_get(&a, 0), ==, 10);
    rc_array_i32_set(&a, 0, 99);
    RC_CHECK(rc_array_i32_get(&a, 0), ==, 99);
    *rc_array_i32_at(&a, 1) = 88;
    RC_CHECK(rc_array_i32_get(&a, 1), ==, 88);
}


RC_TEST_STEP(array, last_at, fix)
{
    rc_array_i32 a = {0};

    rc_array_i32_push(&a, 1, &fix->a);
    RC_CHECK_TRUE(rc_array_i32_last_at(&a) == rc_array_i32_at(&a, 0));
    rc_array_i32_push(&a, 2, &fix->a);
    RC_CHECK(*rc_array_i32_last_at(&a), ==, 2);
    *rc_array_i32_last_at(&a) = 20;
    RC_CHECK(rc_array_i32_get(&a, 1), ==, 20);
}


RC_TEST_STEP(array, rc_at, fix)
{
    // The convenience macro, as an lvalue on arrays, spans and views alike
    rc_array_i32 a = {0};

    rc_array_i32_push(&a, 11, &fix->a);
    rc_array_i32_push(&a, 22, &fix->a);
    RC_CHECK(RC_AT(a, 0), ==, 11);
    RC_AT(a, 1) = 33;
    RC_CHECK(rc_array_i32_get(&a, 1), ==, 33);

    rc_span_i32 s = a.span;
    RC_AT(s, 0) = 44;
    RC_CHECK(RC_AT(a.view, 0), ==, 44);
}


RC_TEST_STEP(array, is_valid_is_empty, fix)
{
    rc_array_i32 a = {0};

    RC_CHECK_FALSE(rc_array_i32_is_valid(&a));
    RC_CHECK_TRUE(rc_array_i32_is_empty(&a));
    rc_array_i32_push(&a, 1, &fix->a);
    RC_CHECK_TRUE(rc_array_i32_is_valid(&a));
    RC_CHECK_FALSE(rc_array_i32_is_empty(&a));
    rc_array_i32_reset(&a);
    RC_CHECK_TRUE(rc_array_i32_is_valid(&a));
    RC_CHECK_TRUE(rc_array_i32_is_empty(&a));
}


// ---- push, pop and append ----

RC_TEST_STEP(array, push, fix)
{
    rc_array_i32 a = {0};

    RC_CHECK(rc_array_i32_push(&a, 5, &fix->a), ==, 0u);
    RC_CHECK(rc_array_i32_push(&a, 6, &fix->a), ==, 1u);
    RC_CHECK(a.num, ==, 2u);
    RC_CHECK(rc_array_i32_get(&a, 0), ==, 5);
    RC_CHECK(rc_array_i32_get(&a, 1), ==, 6);
}


RC_TEST_STEP(array, push_within_capacity_needs_no_arena, fix)
{
    rc_array_i32 a = rc_array_i32_make(4, &fix->a);

    rc_array_i32_push(&a, 1, NULL);
    rc_array_i32_push(&a, 2, NULL);
    RC_CHECK(a.num, ==, 2u);
}


RC_TEST_STEP(array, push_growth, fix)
{
    // The first growth gives 8, then capacity doubles, so 1000 pushes reallocate rarely
    rc_array_i32 a = {0};
    uint32_t grows = 0;

    for (uint32_t i = 0; i < 1000; i++) {
        uint32_t cap = a.cap;

        rc_array_i32_push(&a, (int32_t)i, &fix->a);
        if (a.cap != cap) {
            grows++;
        }
        if (i == 0) {
            RC_CHECK(a.cap, ==, 8u);
        }
    }
    RC_CHECK(grows, ==, 8u);
    for (uint32_t i = 0; i < 1000; i++) {
        RC_CHECK(rc_array_i32_get(&a, i), ==, (int32_t)i);
    }
}


RC_TEST_STEP(array, push_n, fix)
{
    rc_array_i32 a = {0};

    rc_array_i32_push(&a, 1, &fix->a);

    uint32_t first = rc_array_i32_push_n(&a, 3, &fix->a);
    RC_CHECK(first, ==, 1u);
    RC_CHECK(a.num, ==, 4u);
    for (uint32_t i = first; i < a.num; i++) {
        rc_array_i32_set(&a, i, (int32_t)(i * 10));
    }
    RC_CHECK(rc_array_i32_get(&a, 1), ==, 10);
    RC_CHECK(rc_array_i32_get(&a, 3), ==, 30);
}


RC_TEST_STEP(array, push_n_zero, fix)
{
    rc_array_i32 a = {0};

    rc_array_i32_push(&a, 5, &fix->a);

    uint32_t first = rc_array_i32_push_n_zero(&a, 2, &fix->a);
    RC_CHECK(first, ==, 1u);
    RC_CHECK(a.num, ==, 3u);
    RC_CHECK(rc_array_i32_get(&a, 0), ==, 5);
    RC_CHECK(rc_array_i32_get(&a, 1), ==, 0);
    RC_CHECK(rc_array_i32_get(&a, 2), ==, 0);
}


RC_TEST_STEP(array, push_n_none, fix)
{
    rc_array_i32 a = {0};

    push_count(&a, 2, &fix->a);
    RC_CHECK(rc_array_i32_push_n(&a, 0, NULL), ==, 2u);
    RC_CHECK(rc_array_i32_push_n_zero(&a, 0, NULL), ==, 2u);
    RC_CHECK(a.num, ==, 2u);
}


RC_TEST_STEP(array, pop, fix)
{
    rc_array_i32 a = {0};

    rc_array_i32_push(&a, 10, &fix->a);
    rc_array_i32_push(&a, 20, &fix->a);
    RC_CHECK(rc_array_i32_pop(&a), ==, 20);
    RC_CHECK(a.num, ==, 1u);
    RC_CHECK(rc_array_i32_pop(&a), ==, 10);
    RC_CHECK(a.num, ==, 0u);
}


RC_TEST_STEP(array, pop_n, fix)
{
    rc_array_i32 a = {0};

    push_count(&a, 5, &fix->a);
    rc_array_i32_pop_n(&a, 2);
    RC_CHECK(a.num, ==, 3u);
    RC_CHECK(rc_array_i32_get(&a, 2), ==, 2);
    rc_array_i32_pop_n(&a, 0);
    RC_CHECK(a.num, ==, 3u);
    rc_array_i32_pop_n(&a, 3);
    RC_CHECK(a.num, ==, 0u);
}


RC_TEST_STEP(array, append, fix)
{
    int32_t more[] = {2, 3, 4};
    int32_t expect[] = {1, 2, 3, 4};
    rc_view_i32 v = RC_VIEW(more);
    rc_array_i32 a = {0};

    rc_array_i32_push(&a, 1, &fix->a);
    RC_CHECK(rc_array_i32_append(&a, v, &fix->a), ==, 1u);
    RC_CHECK_TRUE(view_is(a.view, expect, 4));
}


RC_TEST_STEP(array, append_empty, fix)
{
    // Appending nothing still reports where it would have gone, and allocates nothing
    rc_array_i32 a = {0};

    RC_CHECK(rc_array_i32_append(&a, (rc_view_i32) {0}, NULL), ==, 0u);
    RC_CHECK_FALSE(rc_array_i32_is_valid(&a));
    push_count(&a, 3, &fix->a);
    RC_CHECK(rc_array_i32_append(&a, (rc_view_i32) {0}, NULL), ==, 3u);
    RC_CHECK(a.num, ==, 3u);
}


RC_TEST_STEP(array, append_other_array, fix)
{
    int32_t expect[] = {9, 8};
    rc_array_i32 a = {0};
    rc_array_i32 b = {0};

    rc_array_i32_push(&b, 9, &fix->a);
    rc_array_i32_push(&b, 8, &fix->a);
    rc_array_i32_append(&a, b.view, &fix->a);
    RC_CHECK_TRUE(view_is(a.view, expect, 2));
}


// ---- reserve, resize, reset and release ----

RC_TEST_STEP(array, reserve, fix)
{
    rc_array_i32 a = {0};

    rc_array_i32_reserve(&a, 100, &fix->a);
    RC_CHECK(a.cap, ==, 100u);
    RC_CHECK(a.num, ==, 0u);

    // Smaller is a no-op, so no arena is needed
    rc_array_i32_reserve(&a, 10, NULL);
    RC_CHECK(a.cap, ==, 100u);
}


RC_TEST_STEP(array, reserve_keeps_elements, fix)
{
    int32_t expect[] = {0, 1, 2};
    rc_array_i32 a = {0};

    push_count(&a, 3, &fix->a);

    // Another allocation after it means the buffer has to move to grow
    rc_arena_alloc(&fix->a, 16);
    rc_array_i32_reserve(&a, 1000, &fix->a);
    RC_CHECK_TRUE(view_is(a.view, expect, 3));
}


RC_TEST_STEP(array, resize_grow, fix)
{
    rc_array_i32 a = {0};

    rc_array_i32_push(&a, 7, &fix->a);

    rc_span_i32 s = rc_array_i32_resize(&a, 4, &fix->a);
    RC_CHECK(a.num, ==, 4u);
    RC_CHECK(a.cap, ==, 8u);
    RC_CHECK(s.num, ==, 4u);
    RC_CHECK_TRUE(s.data == a.data);
    RC_CHECK(rc_span_i32_get(s, 0), ==, 7);
    rc_span_i32_set(s, 3, 102);
    RC_CHECK(rc_array_i32_get(&a, 3), ==, 102);
}


RC_TEST_STEP(array, resize_exact_capacity, fix)
{
    // Unlike push, resize asks for exactly what it needs
    rc_array_i32 a = {0};

    rc_array_i32_resize(&a, 5, &fix->a);
    RC_CHECK(a.cap, ==, 5u);
}


RC_TEST_STEP(array, resize_shrink, fix)
{
    rc_array_i32 a = {0};

    rc_array_i32_resize(&a, 5, &fix->a);

    rc_span_i32 s = rc_array_i32_resize(&a, 2, NULL);
    RC_CHECK(a.num, ==, 2u);
    RC_CHECK(a.cap, ==, 5u);
    RC_CHECK(s.num, ==, 2u);
}


RC_TEST_STEP(array, resize_zero_grow, fix)
{
    int32_t expect[] = {7, 0, 0, 0};
    rc_array_i32 a = {0};

    rc_array_i32_push(&a, 7, &fix->a);

    rc_span_i32 s = rc_array_i32_resize_zero(&a, 4, &fix->a);
    RC_CHECK_TRUE(view_is(s.view, expect, 4));
    RC_CHECK_TRUE(view_is(a.view, expect, 4));
}


RC_TEST_STEP(array, resize_zero_shrink_and_regrow, fix)
{
    int32_t one[] = {1};
    int32_t regrown[] = {1, 0, 0};
    rc_array_i32 a = {0};

    rc_array_i32_push(&a, 1, &fix->a);
    rc_array_i32_push(&a, 2, &fix->a);
    rc_array_i32_push(&a, 3, &fix->a);

    // Shrinking is the same as resize
    rc_span_i32 s = rc_array_i32_resize_zero(&a, 1, NULL);
    RC_CHECK_TRUE(view_is(s.view, one, 1));

    // Growing back within capacity zeroes the stale 2 and 3
    s = rc_array_i32_resize_zero(&a, 3, NULL);
    RC_CHECK_TRUE(view_is(s.view, regrown, 3));
}


RC_TEST_STEP(array, resize_zero_same_size_and_empty, fix)
{
    int32_t five[] = {5};
    int32_t zeros[] = {0, 0, 0};
    rc_array_i32 a = {0};
    rc_array_i32 e = {0};
    rc_array_i32 z = {0};

    // The same size has no new elements to zero
    rc_array_i32_push(&a, 5, &fix->a);
    RC_CHECK_TRUE(view_is(rc_array_i32_resize_zero(&a, 1, NULL).view, five, 1));

    // Growing the zero value zeroes everything
    RC_CHECK_TRUE(view_is(rc_array_i32_resize_zero(&e, 3, &fix->a).view, zeros, 3));

    // Resizing the zero value to nothing needs no arena
    RC_CHECK(rc_array_i32_resize_zero(&z, 0, NULL).num, ==, 0u);
    RC_CHECK_FALSE(rc_array_i32_is_valid(&z));
}


RC_TEST_STEP(array, reset, fix)
{
    rc_array_i32 a = {0};

    push_count(&a, 2, &fix->a);

    uint32_t cap = a.cap;
    rc_array_i32_reset(&a);
    RC_CHECK(a.num, ==, 0u);
    RC_CHECK(a.cap, ==, cap);
}


RC_TEST_STEP(array, release, fix)
{
    rc_array_i32 a = {0};

    push_count(&a, 2, &fix->a);
    rc_array_i32_release(&a, &fix->a);
    RC_CHECK(a.num, ==, 0u);
    RC_CHECK(a.cap, ==, 0u);
    RC_CHECK_TRUE(a.data == NULL);

    // A second release of the zeroed array does nothing
    rc_array_i32_release(&a, &fix->a);
    RC_CHECK_TRUE(a.data == NULL);
}


RC_TEST_STEP(array, release_reclaims_last, fix)
{
    // As the arena's last allocation, the buffer goes straight back
    rc_arena_mark before = rc_arena_save(&fix->a);
    rc_array_i32 a = {0};

    push_count(&a, 100, &fix->a);
    rc_array_i32_release(&a, &fix->a);
    RC_CHECK(rc_arena_save(&fix->a).top, ==, before.top);
}


// ---- insert and remove ----

RC_TEST_STEP(array, insert, fix)
{
    int32_t middle[] = {1, 2, 3};
    int32_t ends[] = {0, 1, 2, 3, 4};
    rc_array_i32 a = {0};

    rc_array_i32_push(&a, 1, &fix->a);
    rc_array_i32_push(&a, 3, &fix->a);
    rc_array_i32_insert(&a, 1, 2, &fix->a);
    RC_CHECK_TRUE(view_is(a.view, middle, 3));
    rc_array_i32_insert(&a, 3, 4, &fix->a);
    rc_array_i32_insert(&a, 0, 0, &fix->a);
    RC_CHECK_TRUE(view_is(a.view, ends, 5));
}


RC_TEST_STEP(array, insert_into_empty, fix)
{
    int32_t expect[] = {42};
    rc_array_i32 a = {0};

    rc_array_i32_insert(&a, 0, 42, &fix->a);
    RC_CHECK_TRUE(view_is(a.view, expect, 1));
}


RC_TEST_STEP(array, insert_n, fix)
{
    int32_t expect[] = {1, 2, 3, 4};
    rc_array_i32 a = {0};

    rc_array_i32_push(&a, 1, &fix->a);
    rc_array_i32_push(&a, 4, &fix->a);
    rc_array_i32_insert_n(&a, 1, 2, &fix->a);
    RC_CHECK(a.num, ==, 4u);
    rc_array_i32_set(&a, 1, 2);
    rc_array_i32_set(&a, 2, 3);
    RC_CHECK_TRUE(view_is(a.view, expect, 4));
}


RC_TEST_STEP(array, insert_n_ends, fix)
{
    int32_t expect[] = {0, 0, 5, 0, 0};
    rc_array_i32 a = {0};

    rc_array_i32_push(&a, 5, &fix->a);
    rc_array_i32_insert_n_zero(&a, 0, 2, &fix->a);
    rc_array_i32_insert_n_zero(&a, 3, 2, &fix->a);
    RC_CHECK_TRUE(view_is(a.view, expect, 5));
}


RC_TEST_STEP(array, insert_n_zero, fix)
{
    int32_t expect[] = {1, 0, 0, 4};
    rc_array_i32 a = {0};

    rc_array_i32_push(&a, 1, &fix->a);
    rc_array_i32_push(&a, 4, &fix->a);
    rc_array_i32_insert_n_zero(&a, 1, 2, &fix->a);
    RC_CHECK_TRUE(view_is(a.view, expect, 4));
}


RC_TEST_STEP(array, insert_n_none, fix)
{
    int32_t expect[] = {0, 1};
    rc_array_i32 a = {0};

    push_count(&a, 2, &fix->a);
    rc_array_i32_insert_n(&a, 1, 0, NULL);
    rc_array_i32_insert_n_zero(&a, 2, 0, NULL);
    RC_CHECK_TRUE(view_is(a.view, expect, 2));
}


RC_TEST_STEP(array, remove, fix)
{
    int32_t middle[] = {0, 2, 3};
    int32_t ends[] = {2};
    rc_array_i32 a = {0};

    push_count(&a, 4, &fix->a);
    rc_array_i32_remove(&a, 1);
    RC_CHECK_TRUE(view_is(a.view, middle, 3));
    rc_array_i32_remove(&a, 2);
    rc_array_i32_remove(&a, 0);
    RC_CHECK_TRUE(view_is(a.view, ends, 1));
    rc_array_i32_remove(&a, 0);
    RC_CHECK(a.num, ==, 0u);
}


RC_TEST_STEP(array, remove_n, fix)
{
    int32_t middle[] = {0, 3, 4};
    int32_t last[] = {0};
    rc_array_i32 a = {0};

    push_count(&a, 5, &fix->a);
    rc_array_i32_remove_n(&a, 1, 2);
    RC_CHECK_TRUE(view_is(a.view, middle, 3));
    rc_array_i32_remove_n(&a, 1, 2);
    RC_CHECK_TRUE(view_is(a.view, last, 1));
    rc_array_i32_remove_n(&a, 0, 1);
    RC_CHECK(a.num, ==, 0u);
}


RC_TEST_STEP(array, remove_n_none, fix)
{
    int32_t expect[] = {0, 1, 2};
    rc_array_i32 a = {0};
    rc_array_i32 z = {0};

    push_count(&a, 3, &fix->a);
    rc_array_i32_remove_n(&a, 1, 0);
    rc_array_i32_remove_n(&a, 3, 0);
    RC_CHECK_TRUE(view_is(a.view, expect, 3));

    // Nothing from nothing, with no buffer at all
    rc_array_i32_remove_n(&z, 0, 0);
    RC_CHECK(z.num, ==, 0u);
}


// ---- conversions ----

RC_TEST(array, view_span_macros)
{
    int32_t raw[] = {5, 6, 7};
    rc_view_i32 v = RC_VIEW(raw);
    rc_span_i32 s = RC_SPAN(raw);

    RC_CHECK(v.num, ==, 3u);
    RC_CHECK(rc_view_i32_get(v, 2), ==, 7);
    RC_CHECK(s.num, ==, 3u);
    rc_span_i32_set(s, 0, 50);
    RC_CHECK(raw[0], ==, 50);
    RC_CHECK(rc_view_i32_get(s.view, 0), ==, 50);
}


RC_TEST_STEP(array, array_to_span_and_view, fix)
{
    rc_array_i32 a = {0};

    rc_array_i32_push(&a, 1, &fix->a);

    rc_span_i32 s = a.span;
    RC_CHECK(s.num, ==, 1u);
    rc_span_i32_set(s, 0, 77);
    RC_CHECK(rc_array_i32_get(&a, 0), ==, 77);
    RC_CHECK(rc_view_i32_get(a.view, 0), ==, 77);
}


RC_TEST_STEP(array, self_referencing_type, fix)
{
    // A node array whose elements view other nodes
    rc_array_node nodes = {0};
    node leaf = {.children = {0}, .id = 1};

    rc_array_node_push(&nodes, leaf, &fix->a);
    rc_array_node_push(&nodes, (node) {.children = nodes.view, .id = 2}, &fix->a);
    RC_CHECK(rc_array_node_get(&nodes, 1).children.num, ==, 1u);
    RC_CHECK(rc_view_node_get(rc_array_node_get(&nodes, 1).children, 0).id, ==, 1);
}


// ---- span ----

RC_TEST(span, from_ptr)
{
    int32_t raw[] = {1, 2, 3};
    rc_span_i32 s = rc_span_i32_from_ptr(raw, 3);

    RC_CHECK(s.num, ==, 3u);
    RC_CHECK(rc_span_i32_get(s, 0), ==, 1);
    rc_span_i32_set(s, 0, 9);
    RC_CHECK(raw[0], ==, 9);
}


RC_TEST(span, make)
{
    rc_arena arena = rc_arena_make_default();
    rc_span_i32 s = rc_span_i32_make(5, &arena);

    RC_CHECK(s.num, ==, 5u);
    RC_CHECK_TRUE(rc_span_i32_is_valid(s));
    for (uint32_t i = 0; i < s.num; i++) {
        rc_span_i32_set(s, i, (int32_t)i);
    }
    RC_CHECK(rc_span_i32_get(s, 4), ==, 4);
    rc_arena_release(&arena);
}


RC_TEST(span, make_zero)
{
    int32_t zeros[] = {0, 0, 0, 0};
    rc_arena arena = rc_arena_make_default();

    // Dirty the memory first, so the zeroing is seen to happen
    rc_span_i32 dirty = rc_span_i32_make(4, &arena);
    for (uint32_t i = 0; i < dirty.num; i++) {
        rc_span_i32_set(dirty, i, -1);
    }
    rc_arena_free(&arena, dirty.data, (uint32_t)(dirty.num * sizeof(int32_t)));

    rc_span_i32 s = rc_span_i32_make_zero(4, &arena);
    RC_CHECK_TRUE(s.data == dirty.data);
    RC_CHECK_TRUE(view_is(s.view, zeros, 4));
    rc_arena_release(&arena);
}


RC_TEST(span, make_empty)
{
    // Nothing is allocated, so no arena is needed
    rc_span_i32 s = rc_span_i32_make(0, NULL);
    rc_span_i32 z = rc_span_i32_make_zero(0, NULL);

    RC_CHECK(s.num, ==, 0u);
    RC_CHECK_FALSE(rc_span_i32_is_valid(s));
    RC_CHECK(z.num, ==, 0u);
    RC_CHECK_FALSE(rc_span_i32_is_valid(z));
}


RC_TEST(span, make_is_exact)
{
    // Two spans made in a row sit end to end: no spare room after the first
    rc_arena arena = rc_arena_make_default();
    rc_span_i32 a = rc_span_i32_make(4, &arena);
    rc_span_i32 b = rc_span_i32_make(4, &arena);

    RC_CHECK_TRUE((uintptr_t)b.data - (uintptr_t)a.data == 4 * sizeof(int32_t));
    rc_arena_release(&arena);
}


RC_TEST(span, is_valid_is_empty)
{
    int32_t raw[] = {1, 2};
    rc_span_i32 s = rc_span_i32_from_ptr(raw, 2);
    rc_span_i32 e = rc_span_i32_from_ptr(NULL, 0);
    rc_span_i32 valid_empty = rc_span_i32_from_ptr(raw, 0);

    RC_CHECK_TRUE(rc_span_i32_is_valid(s));
    RC_CHECK_FALSE(rc_span_i32_is_empty(s));
    RC_CHECK_FALSE(rc_span_i32_is_valid(e));
    RC_CHECK_TRUE(rc_span_i32_is_empty(e));
    RC_CHECK_TRUE(rc_span_i32_is_valid(valid_empty));
    RC_CHECK_TRUE(rc_span_i32_is_empty(valid_empty));
}


RC_TEST(span, first_last)
{
    int32_t raw[] = {0, 1, 2, 3, 4};
    rc_span_i32 s = RC_SPAN(raw);

    RC_CHECK_TRUE(view_is(rc_span_i32_first(s, 2).view, raw, 2));
    RC_CHECK_TRUE(view_is(rc_span_i32_last(s, 2).view, raw + 3, 2));
    RC_CHECK_TRUE(view_is(rc_span_i32_first(s, 99).view, raw, 5));
    RC_CHECK_TRUE(view_is(rc_span_i32_last(s, 99).view, raw, 5));
    RC_CHECK(rc_span_i32_first(s, 0).num, ==, 0u);
    RC_CHECK(rc_span_i32_last(s, 0).num, ==, 0u);
}


RC_TEST(span, drop_first_last)
{
    int32_t raw[] = {0, 1, 2, 3, 4};
    rc_span_i32 s = RC_SPAN(raw);

    RC_CHECK_TRUE(view_is(rc_span_i32_drop_first(s, 2).view, raw + 2, 3));
    RC_CHECK_TRUE(view_is(rc_span_i32_drop_last(s, 2).view, raw, 3));
    RC_CHECK_TRUE(view_is(rc_span_i32_drop_first(s, 0).view, raw, 5));
    RC_CHECK_TRUE(view_is(rc_span_i32_drop_last(s, 0).view, raw, 5));
    RC_CHECK(rc_span_i32_drop_first(s, 99).num, ==, 0u);
    RC_CHECK(rc_span_i32_drop_last(s, 99).num, ==, 0u);
}


RC_TEST(span, subspan)
{
    int32_t raw[] = {0, 1, 2, 3, 4};
    rc_span_i32 s = RC_SPAN(raw);

    RC_CHECK_TRUE(view_is(rc_span_i32_subspan(s, 1, 4).view, raw + 1, 3));
    RC_CHECK_TRUE(view_is(rc_span_i32_subspan(s, 3, 99).view, raw + 3, 2));
    RC_CHECK_TRUE(view_is(rc_span_i32_subspan(s, 0, RC_INDEX_NONE).view, raw, 5));
    RC_CHECK(rc_span_i32_subspan(s, 2, 2).num, ==, 0u);

    // An end before start gives nothing, at start
    RC_CHECK(rc_span_i32_subspan(s, 4, 2).num, ==, 0u);
    RC_CHECK_TRUE(rc_span_i32_subspan(s, 4, 2).data == raw + 4);
    RC_CHECK_TRUE(rc_span_i32_subspan(s, 9, 2).data == raw + 5);
}


RC_TEST(span, slices_write_through)
{
    int32_t raw[] = {0, 1, 2, 3, 4};
    rc_span_i32 s = RC_SPAN(raw);

    rc_span_i32_set(rc_span_i32_last(s, 1), 0, 40);
    rc_span_i32_set(rc_span_i32_subspan(s, 1, 2), 0, 10);
    RC_CHECK(raw[4], ==, 40);
    RC_CHECK(raw[1], ==, 10);
}


RC_TEST(span, get_set_at)
{
    int32_t raw[] = {10, 20, 30};
    rc_span_i32 s = RC_SPAN(raw);

    RC_CHECK(rc_span_i32_get(s, 1), ==, 20);
    rc_span_i32_set(s, 1, 99);
    RC_CHECK(raw[1], ==, 99);
    *rc_span_i32_at(s, 2) = 77;
    RC_CHECK(raw[2], ==, 77);
}


RC_TEST(span, last_at)
{
    int32_t raw[] = {5, 6, 7};
    rc_span_i32 s = RC_SPAN(raw);

    RC_CHECK_TRUE(rc_span_i32_last_at(s) == &raw[2]);
    *rc_span_i32_last_at(s) = 70;
    RC_CHECK(raw[2], ==, 70);
}


RC_TEST(span, reverse)
{
    // Every length up to 33, odd and even, including 0 and 1
    for (uint32_t n = 0; n <= 33; n++) {
        int32_t a[40];

        for (uint32_t i = 0; i < n; i++) {
            a[i] = (int32_t)i;
        }
        rc_span_i32_reverse(rc_span_i32_from_ptr(a, n));
        for (uint32_t i = 0; i < n; i++) {
            RC_CHECK(a[i], ==, (int32_t)(n - 1 - i));
        }
    }
    rc_span_i32_reverse(rc_span_i32_from_ptr(NULL, 0));
}


RC_TEST(span, rotate_example)
{
    int32_t a[] = {1, 2, 3, 4, 5};
    int32_t expect[] = {3, 4, 5, 1, 2};

    rc_span_i32_rotate(rc_span_i32_from_ptr(a, 5), 2);
    RC_CHECK_TRUE(view_is(rc_view_i32_from_ptr(a, 5), expect, 5));
}


RC_TEST(span, rotate_noops)
{
    int32_t a[] = {1, 2, 3, 4};
    int32_t expect[] = {1, 2, 3, 4};
    int32_t one[] = {7};

    // k is not taken modulo num, so k >= num does nothing
    rc_span_i32_rotate(rc_span_i32_from_ptr(a, 4), 0);
    rc_span_i32_rotate(rc_span_i32_from_ptr(a, 4), 4);
    rc_span_i32_rotate(rc_span_i32_from_ptr(a, 4), 100);
    RC_CHECK_TRUE(view_is(rc_view_i32_from_ptr(a, 4), expect, 4));
    rc_span_i32_rotate(rc_span_i32_from_ptr(one, 1), 1);
    RC_CHECK(one[0], ==, 7);
    rc_span_i32_rotate(rc_span_i32_from_ptr(NULL, 0), 3);
}


RC_TEST(span, rotate_exhaustive)
{
    // Rotating left by k puts the element from (i + k) % n at i, for every n and k up to 33
    for (uint32_t n = 0; n <= 33; n++) {
        for (uint32_t k = 0; k <= n + 2; k++) {
            int32_t a[40];

            for (uint32_t i = 0; i < n; i++) {
                a[i] = (int32_t)i;
            }
            rc_span_i32_rotate(rc_span_i32_from_ptr(a, n), k);
            for (uint32_t i = 0; i < n; i++) {
                uint32_t want = (k == 0 || k >= n) ? i : (i + k) % n;

                RC_CHECK(a[i], ==, (int32_t)want);
            }
        }
    }
}


RC_TEST(span, rotate_struct_elements)
{
    pt a[] = {{1, 1}, {2, 2}, {3, 3}, {4, 4}};
    int32_t expect[] = {4, 1, 2, 3};

    rc_span_pt_rotate(rc_span_pt_from_ptr(a, 4), 3);
    for (uint32_t i = 0; i < 4; i++) {
        RC_CHECK(a[i].x, ==, expect[i]);
        RC_CHECK(a[i].y, ==, expect[i]);
    }
}


// ---- view ----

RC_TEST(view, from_ptr)
{
    int32_t raw[] = {1, 2, 3, 4};
    rc_view_i32 v = rc_view_i32_from_ptr(raw, 4);

    RC_CHECK_TRUE(v.data == raw);
    RC_CHECK_TRUE(view_is(v, raw, 4));
}


RC_TEST(view, is_valid_is_empty)
{
    int32_t raw[] = {1, 2};
    rc_view_i32 v = rc_view_i32_from_ptr(raw, 2);
    rc_view_i32 e = rc_view_i32_from_ptr(NULL, 0);
    rc_view_i32 valid_empty = rc_view_i32_from_ptr(raw, 0);

    RC_CHECK_TRUE(rc_view_i32_is_valid(v));
    RC_CHECK_FALSE(rc_view_i32_is_empty(v));
    RC_CHECK_FALSE(rc_view_i32_is_valid(e));
    RC_CHECK_TRUE(rc_view_i32_is_empty(e));
    RC_CHECK_TRUE(rc_view_i32_is_valid(valid_empty));
    RC_CHECK_TRUE(rc_view_i32_is_empty(valid_empty));
}


RC_TEST(view, first_last)
{
    int32_t raw[] = {0, 1, 2, 3, 4};
    rc_view_i32 v = RC_VIEW(raw);

    RC_CHECK_TRUE(view_is(rc_view_i32_first(v, 2), raw, 2));
    RC_CHECK_TRUE(view_is(rc_view_i32_last(v, 2), raw + 3, 2));
    RC_CHECK_TRUE(view_is(rc_view_i32_first(v, 99), raw, 5));
    RC_CHECK_TRUE(view_is(rc_view_i32_last(v, 99), raw, 5));
    RC_CHECK(rc_view_i32_first(v, 0).num, ==, 0u);
    RC_CHECK(rc_view_i32_last(v, 0).num, ==, 0u);
}


RC_TEST(view, drop_first_last)
{
    int32_t raw[] = {0, 1, 2, 3, 4};
    rc_view_i32 v = RC_VIEW(raw);

    RC_CHECK_TRUE(view_is(rc_view_i32_drop_first(v, 2), raw + 2, 3));
    RC_CHECK_TRUE(view_is(rc_view_i32_drop_last(v, 2), raw, 3));
    RC_CHECK_TRUE(view_is(rc_view_i32_drop_first(v, 0), raw, 5));
    RC_CHECK_TRUE(view_is(rc_view_i32_drop_last(v, 0), raw, 5));
    RC_CHECK(rc_view_i32_drop_first(v, 99).num, ==, 0u);
    RC_CHECK(rc_view_i32_drop_last(v, 99).num, ==, 0u);
}


RC_TEST(view, first_and_drop_first_partition)
{
    // Cutting at n with first and drop_first loses nothing, and the same for last
    int32_t raw[] = {0, 1, 2, 3, 4, 5};
    rc_view_i32 v = RC_VIEW(raw);

    for (uint32_t n = 0; n <= 7; n++) {
        RC_CHECK(rc_view_i32_first(v, n).num + rc_view_i32_drop_first(v, n).num, ==, v.num);
        RC_CHECK(rc_view_i32_last(v, n).num + rc_view_i32_drop_last(v, n).num, ==, v.num);
    }
}


RC_TEST(view, subview)
{
    int32_t raw[] = {0, 1, 2, 3, 4};
    rc_view_i32 v = RC_VIEW(raw);

    RC_CHECK_TRUE(view_is(rc_view_i32_subview(v, 1, 4), raw + 1, 3));
    RC_CHECK_TRUE(view_is(rc_view_i32_subview(v, 3, 99), raw + 3, 2));
    RC_CHECK_TRUE(view_is(rc_view_i32_subview(v, 0, RC_INDEX_NONE), raw, 5));
    RC_CHECK(rc_view_i32_subview(v, 2, 2).num, ==, 0u);
    RC_CHECK(rc_view_i32_subview(v, 4, 2).num, ==, 0u);
    RC_CHECK_TRUE(rc_view_i32_subview(v, 4, 2).data == raw + 4);
}


RC_TEST(view, get_at)
{
    int32_t raw[] = {10, 20, 30};
    rc_view_i32 v = RC_VIEW(raw);

    RC_CHECK(rc_view_i32_get(v, 1), ==, 20);
    RC_CHECK_TRUE(rc_view_i32_at(v, 0) == &raw[0]);
}


RC_TEST(view, last_at)
{
    int32_t raw[] = {5, 6, 7};
    rc_view_i32 v = RC_VIEW(raw);

    RC_CHECK_TRUE(rc_view_i32_last_at(v) == &raw[2]);
    RC_CHECK(*rc_view_i32_last_at(v), ==, 7);
}
