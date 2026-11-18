// test_box2f.c
//
// Tests for rc_box2f, a half-open box of floats.

#include "richc/math/box2f.h"

#include "richc/test.h"


// ---- construction ----

RC_TEST(box2f, make_sorts_the_corners)
{
    rc_box2f box = rc_box2f_make(rc_vec2f_make(5.0f, 1.0f), rc_vec2f_make(2.0f, 8.0f));

    RC_CHECK(rc_box2f_min(box), ==, rc_vec2f_make(2.0f, 1.0f));
    RC_CHECK(rc_box2f_max(box), ==, rc_vec2f_make(5.0f, 8.0f));
    RC_CHECK_TRUE(rc_box2f_is_equal(box, rc_box2f_make(rc_vec2f_make(2.0f, 8.0f), rc_vec2f_make(5.0f, 1.0f))));
}


RC_TEST(box2f, make_pos_size)
{
    rc_box2f box = rc_box2f_make_pos_size(rc_vec2f_make(3.0f, -4.0f), rc_vec2f_make(10.0f, 20.0f));

    RC_CHECK(rc_box2f_min(box), ==, rc_vec2f_make(3.0f, -4.0f));
    RC_CHECK(rc_box2f_max(box), ==, rc_vec2f_make(13.0f, 16.0f));
    RC_CHECK(rc_box2f_size(box), ==, rc_vec2f_make(10.0f, 20.0f));
}


RC_TEST(box2f, make_with_margin)
{
    rc_box2f box = rc_box2f_make_with_margin(rc_vec2f_make(5.0f, 3.0f), rc_vec2f_make(2.0f, 7.0f), 1.0f);
    rc_box2f unchanged = rc_box2f_make_with_margin(rc_vec2f_make(5.0f, 3.0f), rc_vec2f_make(2.0f, 7.0f), 0.0f);

    RC_CHECK(rc_box2f_min(box), ==, rc_vec2f_make(1.0f, 2.0f));
    RC_CHECK(rc_box2f_max(box), ==, rc_vec2f_make(6.0f, 8.0f));
    RC_CHECK_TRUE(rc_box2f_is_equal(unchanged, rc_box2f_make(rc_vec2f_make(2.0f, 3.0f), rc_vec2f_make(5.0f, 7.0f))));
}


RC_TEST(box2f, make_with_a_fractional_margin)
{
    rc_box2f box = rc_box2f_make_with_margin(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(1.0f, 2.0f), 0.25f);

    RC_CHECK(rc_box2f_min(box), ==, rc_vec2f_make(-0.25f, -0.25f));
    RC_CHECK(rc_box2f_max(box), ==, rc_vec2f_make(1.25f, 2.25f));
    RC_CHECK(rc_box2f_size(box), ==, rc_vec2f_make(1.5f, 2.5f));
}


// ---- queries ----

RC_TEST(box2f, is_empty)
{
    rc_box2f no_width = rc_box2f_make_pos_size(rc_vec2f_make(3.0f, 3.0f), rc_vec2f_make(0.0f, 5.0f));
    rc_box2f no_height = rc_box2f_make_pos_size(rc_vec2f_make(3.0f, 3.0f), rc_vec2f_make(5.0f, 0.0f));
    rc_box2f unit = rc_box2f_make_pos_size(rc_vec2f_make(3.0f, 3.0f), rc_vec2f_make(1.0f, 1.0f));

    RC_CHECK_TRUE(rc_box2f_is_empty(no_width));
    RC_CHECK_TRUE(rc_box2f_is_empty(no_height));
    RC_CHECK_TRUE(rc_box2f_is_empty(rc_box2f_make(rc_vec2f_make(4.0f, 4.0f), rc_vec2f_make(4.0f, 4.0f))));
    RC_CHECK_FALSE(rc_box2f_is_empty(unit));
    RC_CHECK_FALSE(rc_box2f_is_empty(rc_box2f_make_pos_size(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(4.0f, 4.0f))));
}


RC_TEST(box2f, contains)
{
    rc_box2f outer = rc_box2f_make_pos_size(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(10.0f, 10.0f));
    rc_box2f inner = rc_box2f_make_pos_size(rc_vec2f_make(2.0f, 2.0f), rc_vec2f_make(3.0f, 3.0f));
    rc_box2f poking_out = rc_box2f_make_pos_size(rc_vec2f_make(8.0f, 8.0f), rc_vec2f_make(5.0f, 5.0f));

    RC_CHECK_TRUE(rc_box2f_contains(outer, inner));
    RC_CHECK_TRUE(rc_box2f_contains(outer, outer));
    RC_CHECK_FALSE(rc_box2f_contains(inner, outer));
    RC_CHECK_FALSE(rc_box2f_contains(outer, poking_out));
}


RC_TEST(box2f, contains_up_to_the_edges)
{
    rc_box2f outer = rc_box2f_make_pos_size(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(10.0f, 10.0f));
    rc_box2f flush_min = rc_box2f_make_pos_size(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(5.0f, 5.0f));
    rc_box2f flush_max = rc_box2f_make_pos_size(rc_vec2f_make(5.0f, 5.0f), rc_vec2f_make(5.0f, 5.0f));
    rc_box2f one_past_x = rc_box2f_make_pos_size(rc_vec2f_make(6.0f, 5.0f), rc_vec2f_make(5.0f, 5.0f));
    rc_box2f one_before_y = rc_box2f_make_pos_size(rc_vec2f_make(0.0f, -1.0f), rc_vec2f_make(5.0f, 5.0f));

    // Both being half open, a box sharing either edge still fits
    RC_CHECK_TRUE(rc_box2f_contains(outer, flush_min));
    RC_CHECK_TRUE(rc_box2f_contains(outer, flush_max));
    RC_CHECK_FALSE(rc_box2f_contains(outer, one_past_x));
    RC_CHECK_FALSE(rc_box2f_contains(outer, one_before_y));
}


RC_TEST(box2f, intersects)
{
    rc_box2f box = rc_box2f_make_pos_size(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(10.0f, 10.0f));
    rc_box2f overlapping = rc_box2f_make_pos_size(rc_vec2f_make(5.0f, 5.0f), rc_vec2f_make(10.0f, 10.0f));
    rc_box2f inner = rc_box2f_make_pos_size(rc_vec2f_make(2.0f, 2.0f), rc_vec2f_make(3.0f, 3.0f));
    rc_box2f far = rc_box2f_make_pos_size(rc_vec2f_make(20.0f, 20.0f), rc_vec2f_make(5.0f, 5.0f));
    rc_box2f level_but_far = rc_box2f_make_pos_size(rc_vec2f_make(20.0f, 0.0f), rc_vec2f_make(5.0f, 10.0f));

    RC_CHECK_TRUE(rc_box2f_intersects(box, overlapping));
    RC_CHECK_TRUE(rc_box2f_intersects(overlapping, box));
    RC_CHECK_TRUE(rc_box2f_intersects(box, inner));
    RC_CHECK_TRUE(rc_box2f_intersects(box, box));
    RC_CHECK_FALSE(rc_box2f_intersects(box, far));
    RC_CHECK_FALSE(rc_box2f_intersects(box, level_but_far));
}


RC_TEST(box2f, touching_boxes_dont_intersect)
{
    rc_box2f box = rc_box2f_make_pos_size(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(10.0f, 10.0f));
    rc_box2f right = rc_box2f_make_pos_size(rc_vec2f_make(10.0f, 0.0f), rc_vec2f_make(5.0f, 10.0f));
    rc_box2f below = rc_box2f_make_pos_size(rc_vec2f_make(0.0f, 10.0f), rc_vec2f_make(10.0f, 5.0f));
    rc_box2f left = rc_box2f_make_pos_size(rc_vec2f_make(-5.0f, 0.0f), rc_vec2f_make(5.0f, 10.0f));
    rc_box2f above = rc_box2f_make_pos_size(rc_vec2f_make(0.0f, -5.0f), rc_vec2f_make(10.0f, 5.0f));
    rc_box2f corner = rc_box2f_make_pos_size(rc_vec2f_make(10.0f, 10.0f), rc_vec2f_make(5.0f, 5.0f));
    rc_box2f overlapping_corner = rc_box2f_make_pos_size(rc_vec2f_make(9.0f, 9.0f), rc_vec2f_make(5.0f, 5.0f));

    RC_CHECK_FALSE(rc_box2f_intersects(box, right));
    RC_CHECK_FALSE(rc_box2f_intersects(box, below));
    RC_CHECK_FALSE(rc_box2f_intersects(box, left));
    RC_CHECK_FALSE(rc_box2f_intersects(box, above));
    RC_CHECK_FALSE(rc_box2f_intersects(box, corner));
    RC_CHECK_FALSE(rc_box2f_intersects(right, box));
    RC_CHECK_TRUE(rc_box2f_intersects(box, overlapping_corner));
}


RC_TEST(box2f, contains_point)
{
    rc_box2f box = rc_box2f_make_pos_size(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(10.0f, 10.0f));
    rc_box2f empty = rc_box2f_make_pos_size(rc_vec2f_make(5.0f, 5.0f), rc_vec2f_make(0.0f, 0.0f));

    // min is inside, max is not
    RC_CHECK_TRUE(rc_box2f_contains_point(box, rc_vec2f_make(0.0f, 0.0f)));
    RC_CHECK_TRUE(rc_box2f_contains_point(box, rc_vec2f_make(9.0f, 9.0f)));
    RC_CHECK_TRUE(rc_box2f_contains_point(box, rc_vec2f_make(0.0f, 9.0f)));
    RC_CHECK_TRUE(rc_box2f_contains_point(box, rc_vec2f_make(9.999f, 9.999f)));
    RC_CHECK_FALSE(rc_box2f_contains_point(box, rc_vec2f_make(10.0f, 5.0f)));
    RC_CHECK_FALSE(rc_box2f_contains_point(box, rc_vec2f_make(5.0f, 10.0f)));
    RC_CHECK_FALSE(rc_box2f_contains_point(box, rc_vec2f_make(10.0f, 10.0f)));
    RC_CHECK_FALSE(rc_box2f_contains_point(box, rc_vec2f_make(-1.0f, 5.0f)));
    RC_CHECK_FALSE(rc_box2f_contains_point(box, rc_vec2f_make(5.0f, -1.0f)));
    RC_CHECK_FALSE(rc_box2f_contains_point(empty, rc_vec2f_make(5.0f, 5.0f)));
}


// ---- combination ----

RC_TEST(box2f, union)
{
    rc_box2f a = rc_box2f_make(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(5.0f, 5.0f));
    rc_box2f b = rc_box2f_make(rc_vec2f_make(3.0f, 3.0f), rc_vec2f_make(10.0f, 8.0f));
    rc_box2f apart = rc_box2f_make(rc_vec2f_make(-4.0f, 20.0f), rc_vec2f_make(-2.0f, 30.0f));
    rc_box2f joined = rc_box2f_union(a, b);
    rc_box2f joined_apart = rc_box2f_union(a, apart);

    RC_CHECK(rc_box2f_min(joined), ==, rc_vec2f_make(0.0f, 0.0f));
    RC_CHECK(rc_box2f_max(joined), ==, rc_vec2f_make(10.0f, 8.0f));
    RC_CHECK_TRUE(rc_box2f_is_equal(rc_box2f_union(b, a), joined));
    RC_CHECK_TRUE(rc_box2f_is_equal(rc_box2f_union(a, a), a));
    RC_CHECK(rc_box2f_min(joined_apart), ==, rc_vec2f_make(-4.0f, 0.0f));
    RC_CHECK(rc_box2f_max(joined_apart), ==, rc_vec2f_make(5.0f, 30.0f));
}


RC_TEST(box2f, expand)
{
    rc_box2f box = rc_box2f_make(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(5.0f, 5.0f));
    rc_box2f grown = rc_box2f_expand(box, rc_vec2f_make(-2.0f, 7.0f));

    RC_CHECK(rc_box2f_min(grown), ==, rc_vec2f_make(-2.0f, 0.0f));
    RC_CHECK(rc_box2f_max(grown), ==, rc_vec2f_make(5.0f, 7.0f));
    RC_CHECK_TRUE(rc_box2f_is_equal(rc_box2f_expand(box, rc_vec2f_make(2.0f, 2.0f)), box));
}


RC_TEST(box2f, intersection)
{
    rc_box2f a = rc_box2f_make_pos_size(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(10.0f, 10.0f));
    rc_box2f b = rc_box2f_make_pos_size(rc_vec2f_make(5.0f, 5.0f), rc_vec2f_make(10.0f, 10.0f));
    rc_box2f inner = rc_box2f_make_pos_size(rc_vec2f_make(2.0f, 2.0f), rc_vec2f_make(3.0f, 3.0f));
    rc_box2f overlap = rc_box2f_intersection(a, b);

    RC_CHECK(rc_box2f_min(overlap), ==, rc_vec2f_make(5.0f, 5.0f));
    RC_CHECK(rc_box2f_max(overlap), ==, rc_vec2f_make(10.0f, 10.0f));
    RC_CHECK_TRUE(rc_box2f_is_equal(rc_box2f_intersection(b, a), overlap));
    RC_CHECK_TRUE(rc_box2f_is_equal(rc_box2f_intersection(a, inner), inner));
    RC_CHECK_TRUE(rc_box2f_is_equal(rc_box2f_intersection(a, a), a));
}


RC_TEST(box2f, intersection_of_apart_boxes_is_empty)
{
    rc_box2f box = rc_box2f_make_pos_size(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(10.0f, 10.0f));
    rc_box2f far = rc_box2f_make_pos_size(rc_vec2f_make(20.0f, 20.0f), rc_vec2f_make(5.0f, 5.0f));
    rc_box2f strip = rc_box2f_make_pos_size(rc_vec2f_make(5.0f, 20.0f), rc_vec2f_make(2.0f, 2.0f));
    rc_box2f touching = rc_box2f_make_pos_size(rc_vec2f_make(10.0f, 0.0f), rc_vec2f_make(5.0f, 10.0f));
    rc_box2f none = rc_box2f_intersection(box, far);
    rc_box2f edge = rc_box2f_intersection(box, touching);

    // Still a proper box, with max pulled back to min
    RC_CHECK_TRUE(rc_box2f_is_empty(none));
    RC_CHECK(rc_box2f_min(none), ==, rc_vec2f_make(20.0f, 20.0f));
    RC_CHECK(rc_box2f_max(none), ==, rc_vec2f_make(20.0f, 20.0f));

    // Overlapping on one axis only, or just touching, is still empty
    RC_CHECK_TRUE(rc_box2f_is_empty(rc_box2f_intersection(box, strip)));
    RC_CHECK_TRUE(rc_box2f_is_empty(edge));
    RC_CHECK(rc_box2f_size(edge), ==, rc_vec2f_make(0.0f, 10.0f));
}


// ---- transformation and comparison ----

RC_TEST(box2f, translate)
{
    rc_box2f box = rc_box2f_make_pos_size(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(4.0f, 6.0f));
    rc_box2f moved = rc_box2f_translate(box, rc_vec2f_make(3.0f, -5.0f));

    RC_CHECK_TRUE(rc_box2f_is_equal(moved, rc_box2f_make_pos_size(rc_vec2f_make(4.0f, -3.0f), rc_vec2f_make(4.0f, 6.0f))));
    RC_CHECK(rc_box2f_size(moved), ==, rc_box2f_size(box));
    RC_CHECK_TRUE(rc_box2f_is_equal(rc_box2f_translate(box, rc_vec2f_make_zero()), box));
}


RC_TEST(box2f, is_equal)
{
    rc_box2f box = rc_box2f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(4.0f, 6.0f));
    rc_box2f other_min = rc_box2f_make(rc_vec2f_make(0.0f, 2.0f), rc_vec2f_make(4.0f, 6.0f));
    rc_box2f other_max = rc_box2f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(4.0f, 7.0f));

    RC_CHECK_TRUE(rc_box2f_is_equal(box, rc_box2f_make_pos_size(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.0f, 4.0f))));
    RC_CHECK_TRUE(rc_box2f_is_equal(box, box));
    RC_CHECK_FALSE(rc_box2f_is_equal(box, other_min));
    RC_CHECK_FALSE(rc_box2f_is_equal(box, other_max));
}


RC_TEST(box2f, is_nearly_equal)
{
    rc_box2f box = rc_box2f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.0f, 4.0f));
    rc_box2f nudged = rc_box2f_make(rc_vec2f_make(1.00001f, 1.99999f), rc_vec2f_make(2.99999f, 4.00001f));
    rc_box2f max_off = rc_box2f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.1f, 4.0f));
    rc_box2f min_off = rc_box2f_make(rc_vec2f_make(1.0f, 1.9f), rc_vec2f_make(3.0f, 4.0f));

    RC_CHECK_TRUE(rc_box2f_is_nearly_equal(box, nudged, 0.001f));
    RC_CHECK_FALSE(rc_box2f_is_equal(box, nudged));
    RC_CHECK_FALSE(rc_box2f_is_nearly_equal(box, max_off, 0.001f));
    RC_CHECK_FALSE(rc_box2f_is_nearly_equal(box, min_off, 0.001f));
    RC_CHECK_TRUE(rc_box2f_is_nearly_equal(box, max_off, 0.2f));
}
