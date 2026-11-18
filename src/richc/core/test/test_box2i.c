// test_box2i.c
//
// Tests for rc_box2i, a half-open box of cells.

#include "richc/math/box2i.h"

#include "richc/test.h"


// ---- construction ----

RC_TEST(box2i, make_sorts_the_corners)
{
    rc_box2i box = rc_box2i_make(rc_vec2i_make(5, 1), rc_vec2i_make(2, 8));

    RC_CHECK(rc_box2i_min(box), ==, rc_vec2i_make(2, 1));
    RC_CHECK(rc_box2i_max(box), ==, rc_vec2i_make(5, 8));
    RC_CHECK_TRUE(rc_box2i_is_equal(box, rc_box2i_make(rc_vec2i_make(2, 8), rc_vec2i_make(5, 1))));
}


RC_TEST(box2i, make_pos_size)
{
    rc_box2i box = rc_box2i_make_pos_size(rc_vec2i_make(3, -4), rc_vec2i_make(10, 20));

    RC_CHECK(rc_box2i_min(box), ==, rc_vec2i_make(3, -4));
    RC_CHECK(rc_box2i_max(box), ==, rc_vec2i_make(13, 16));
    RC_CHECK(rc_box2i_size(box), ==, rc_vec2i_make(10, 20));
}


RC_TEST(box2i, make_with_margin)
{
    rc_box2i box = rc_box2i_make_with_margin(rc_vec2i_make(5, 3), rc_vec2i_make(2, 7), 1);
    rc_box2i unchanged = rc_box2i_make_with_margin(rc_vec2i_make(5, 3), rc_vec2i_make(2, 7), 0);

    RC_CHECK(rc_box2i_min(box), ==, rc_vec2i_make(1, 2));
    RC_CHECK(rc_box2i_max(box), ==, rc_vec2i_make(6, 8));
    RC_CHECK_TRUE(rc_box2i_is_equal(unchanged, rc_box2i_make(rc_vec2i_make(2, 3), rc_vec2i_make(5, 7))));
}


RC_TEST(box2i, make_with_margin_to_the_limits)
{
    rc_box2i box = rc_box2i_make_with_margin(rc_vec2i_make(INT32_MIN + 1, -1), rc_vec2i_make(INT32_MAX - 1, 1), 1);

    RC_CHECK(rc_box2i_min(box), ==, rc_vec2i_make(INT32_MIN, -2));
    RC_CHECK(rc_box2i_max(box), ==, rc_vec2i_make(INT32_MAX, 2));
}


// ---- queries ----

RC_TEST(box2i, is_empty)
{
    rc_box2i no_width = rc_box2i_make_pos_size(rc_vec2i_make(3, 3), rc_vec2i_make(0, 5));
    rc_box2i no_height = rc_box2i_make_pos_size(rc_vec2i_make(3, 3), rc_vec2i_make(5, 0));
    rc_box2i one_cell = rc_box2i_make_pos_size(rc_vec2i_make(3, 3), rc_vec2i_make(1, 1));

    RC_CHECK_TRUE(rc_box2i_is_empty(no_width));
    RC_CHECK_TRUE(rc_box2i_is_empty(no_height));
    RC_CHECK_TRUE(rc_box2i_is_empty(rc_box2i_make(rc_vec2i_make(4, 4), rc_vec2i_make(4, 4))));
    RC_CHECK_FALSE(rc_box2i_is_empty(one_cell));
    RC_CHECK_FALSE(rc_box2i_is_empty(rc_box2i_make_pos_size(rc_vec2i_make(0, 0), rc_vec2i_make(4, 4))));
}


RC_TEST(box2i, contains)
{
    rc_box2i outer = rc_box2i_make_pos_size(rc_vec2i_make(0, 0), rc_vec2i_make(10, 10));
    rc_box2i inner = rc_box2i_make_pos_size(rc_vec2i_make(2, 2), rc_vec2i_make(3, 3));
    rc_box2i poking_out = rc_box2i_make_pos_size(rc_vec2i_make(8, 8), rc_vec2i_make(5, 5));

    RC_CHECK_TRUE(rc_box2i_contains(outer, inner));
    RC_CHECK_TRUE(rc_box2i_contains(outer, outer));
    RC_CHECK_FALSE(rc_box2i_contains(inner, outer));
    RC_CHECK_FALSE(rc_box2i_contains(outer, poking_out));
}


RC_TEST(box2i, contains_up_to_the_edges)
{
    rc_box2i outer = rc_box2i_make_pos_size(rc_vec2i_make(0, 0), rc_vec2i_make(10, 10));
    rc_box2i flush_min = rc_box2i_make_pos_size(rc_vec2i_make(0, 0), rc_vec2i_make(5, 5));
    rc_box2i flush_max = rc_box2i_make_pos_size(rc_vec2i_make(5, 5), rc_vec2i_make(5, 5));
    rc_box2i one_past_x = rc_box2i_make_pos_size(rc_vec2i_make(6, 5), rc_vec2i_make(5, 5));
    rc_box2i one_before_y = rc_box2i_make_pos_size(rc_vec2i_make(0, -1), rc_vec2i_make(5, 5));

    // Both being half open, a box sharing either edge still fits
    RC_CHECK_TRUE(rc_box2i_contains(outer, flush_min));
    RC_CHECK_TRUE(rc_box2i_contains(outer, flush_max));
    RC_CHECK_FALSE(rc_box2i_contains(outer, one_past_x));
    RC_CHECK_FALSE(rc_box2i_contains(outer, one_before_y));
}


RC_TEST(box2i, intersects)
{
    rc_box2i box = rc_box2i_make_pos_size(rc_vec2i_make(0, 0), rc_vec2i_make(10, 10));
    rc_box2i overlapping = rc_box2i_make_pos_size(rc_vec2i_make(5, 5), rc_vec2i_make(10, 10));
    rc_box2i inner = rc_box2i_make_pos_size(rc_vec2i_make(2, 2), rc_vec2i_make(3, 3));
    rc_box2i far = rc_box2i_make_pos_size(rc_vec2i_make(20, 20), rc_vec2i_make(5, 5));
    rc_box2i level_but_far = rc_box2i_make_pos_size(rc_vec2i_make(20, 0), rc_vec2i_make(5, 10));

    RC_CHECK_TRUE(rc_box2i_intersects(box, overlapping));
    RC_CHECK_TRUE(rc_box2i_intersects(overlapping, box));
    RC_CHECK_TRUE(rc_box2i_intersects(box, inner));
    RC_CHECK_TRUE(rc_box2i_intersects(box, box));
    RC_CHECK_FALSE(rc_box2i_intersects(box, far));
    RC_CHECK_FALSE(rc_box2i_intersects(box, level_but_far));
}


RC_TEST(box2i, touching_boxes_dont_intersect)
{
    rc_box2i box = rc_box2i_make_pos_size(rc_vec2i_make(0, 0), rc_vec2i_make(10, 10));
    rc_box2i right = rc_box2i_make_pos_size(rc_vec2i_make(10, 0), rc_vec2i_make(5, 10));
    rc_box2i below = rc_box2i_make_pos_size(rc_vec2i_make(0, 10), rc_vec2i_make(10, 5));
    rc_box2i left = rc_box2i_make_pos_size(rc_vec2i_make(-5, 0), rc_vec2i_make(5, 10));
    rc_box2i above = rc_box2i_make_pos_size(rc_vec2i_make(0, -5), rc_vec2i_make(10, 5));
    rc_box2i corner = rc_box2i_make_pos_size(rc_vec2i_make(10, 10), rc_vec2i_make(5, 5));
    rc_box2i one_cell_over = rc_box2i_make_pos_size(rc_vec2i_make(9, 9), rc_vec2i_make(5, 5));

    RC_CHECK_FALSE(rc_box2i_intersects(box, right));
    RC_CHECK_FALSE(rc_box2i_intersects(box, below));
    RC_CHECK_FALSE(rc_box2i_intersects(box, left));
    RC_CHECK_FALSE(rc_box2i_intersects(box, above));
    RC_CHECK_FALSE(rc_box2i_intersects(box, corner));
    RC_CHECK_FALSE(rc_box2i_intersects(right, box));
    RC_CHECK_TRUE(rc_box2i_intersects(box, one_cell_over));
}


RC_TEST(box2i, contains_point)
{
    rc_box2i box = rc_box2i_make_pos_size(rc_vec2i_make(0, 0), rc_vec2i_make(10, 10));
    rc_box2i empty = rc_box2i_make_pos_size(rc_vec2i_make(5, 5), rc_vec2i_make(0, 0));

    // min is inside, max is not
    RC_CHECK_TRUE(rc_box2i_contains_point(box, rc_vec2i_make(0, 0)));
    RC_CHECK_TRUE(rc_box2i_contains_point(box, rc_vec2i_make(9, 9)));
    RC_CHECK_TRUE(rc_box2i_contains_point(box, rc_vec2i_make(0, 9)));
    RC_CHECK_FALSE(rc_box2i_contains_point(box, rc_vec2i_make(10, 5)));
    RC_CHECK_FALSE(rc_box2i_contains_point(box, rc_vec2i_make(5, 10)));
    RC_CHECK_FALSE(rc_box2i_contains_point(box, rc_vec2i_make(10, 10)));
    RC_CHECK_FALSE(rc_box2i_contains_point(box, rc_vec2i_make(-1, 5)));
    RC_CHECK_FALSE(rc_box2i_contains_point(box, rc_vec2i_make(5, -1)));
    RC_CHECK_FALSE(rc_box2i_contains_point(empty, rc_vec2i_make(5, 5)));
}


// ---- combination ----

RC_TEST(box2i, union)
{
    rc_box2i a = rc_box2i_make(rc_vec2i_make(0, 0), rc_vec2i_make(5, 5));
    rc_box2i b = rc_box2i_make(rc_vec2i_make(3, 3), rc_vec2i_make(10, 8));
    rc_box2i apart = rc_box2i_make(rc_vec2i_make(-4, 20), rc_vec2i_make(-2, 30));
    rc_box2i joined = rc_box2i_union(a, b);
    rc_box2i joined_apart = rc_box2i_union(a, apart);

    RC_CHECK(rc_box2i_min(joined), ==, rc_vec2i_make(0, 0));
    RC_CHECK(rc_box2i_max(joined), ==, rc_vec2i_make(10, 8));
    RC_CHECK_TRUE(rc_box2i_is_equal(rc_box2i_union(b, a), joined));
    RC_CHECK_TRUE(rc_box2i_is_equal(rc_box2i_union(a, a), a));
    RC_CHECK(rc_box2i_min(joined_apart), ==, rc_vec2i_make(-4, 0));
    RC_CHECK(rc_box2i_max(joined_apart), ==, rc_vec2i_make(5, 30));
}


RC_TEST(box2i, expand)
{
    rc_box2i box = rc_box2i_make(rc_vec2i_make(0, 0), rc_vec2i_make(5, 5));
    rc_box2i grown = rc_box2i_expand(box, rc_vec2i_make(-2, 7));

    RC_CHECK(rc_box2i_min(grown), ==, rc_vec2i_make(-2, 0));
    RC_CHECK(rc_box2i_max(grown), ==, rc_vec2i_make(5, 7));
    RC_CHECK_TRUE(rc_box2i_is_equal(rc_box2i_expand(box, rc_vec2i_make(2, 2)), box));
}


RC_TEST(box2i, intersection)
{
    rc_box2i a = rc_box2i_make_pos_size(rc_vec2i_make(0, 0), rc_vec2i_make(10, 10));
    rc_box2i b = rc_box2i_make_pos_size(rc_vec2i_make(5, 5), rc_vec2i_make(10, 10));
    rc_box2i inner = rc_box2i_make_pos_size(rc_vec2i_make(2, 2), rc_vec2i_make(3, 3));
    rc_box2i overlap = rc_box2i_intersection(a, b);

    RC_CHECK(rc_box2i_min(overlap), ==, rc_vec2i_make(5, 5));
    RC_CHECK(rc_box2i_max(overlap), ==, rc_vec2i_make(10, 10));
    RC_CHECK_TRUE(rc_box2i_is_equal(rc_box2i_intersection(b, a), overlap));
    RC_CHECK_TRUE(rc_box2i_is_equal(rc_box2i_intersection(a, inner), inner));
    RC_CHECK_TRUE(rc_box2i_is_equal(rc_box2i_intersection(a, a), a));
}


RC_TEST(box2i, intersection_of_apart_boxes_is_empty)
{
    rc_box2i box = rc_box2i_make_pos_size(rc_vec2i_make(0, 0), rc_vec2i_make(10, 10));
    rc_box2i far = rc_box2i_make_pos_size(rc_vec2i_make(20, 20), rc_vec2i_make(5, 5));
    rc_box2i strip = rc_box2i_make_pos_size(rc_vec2i_make(5, 20), rc_vec2i_make(2, 2));
    rc_box2i touching = rc_box2i_make_pos_size(rc_vec2i_make(10, 0), rc_vec2i_make(5, 10));
    rc_box2i none = rc_box2i_intersection(box, far);
    rc_box2i edge = rc_box2i_intersection(box, touching);

    // Still a proper box, with max pulled back to min
    RC_CHECK_TRUE(rc_box2i_is_empty(none));
    RC_CHECK(rc_box2i_min(none), ==, rc_vec2i_make(20, 20));
    RC_CHECK(rc_box2i_max(none), ==, rc_vec2i_make(20, 20));

    // Overlapping on one axis only, or just touching, is still empty
    RC_CHECK_TRUE(rc_box2i_is_empty(rc_box2i_intersection(box, strip)));
    RC_CHECK_TRUE(rc_box2i_is_empty(edge));
    RC_CHECK(rc_box2i_size(edge), ==, rc_vec2i_make(0, 10));
}


// ---- transformation and comparison ----

RC_TEST(box2i, translate)
{
    rc_box2i box = rc_box2i_make_pos_size(rc_vec2i_make(1, 2), rc_vec2i_make(4, 6));
    rc_box2i moved = rc_box2i_translate(box, rc_vec2i_make(3, -5));

    RC_CHECK_TRUE(rc_box2i_is_equal(moved, rc_box2i_make_pos_size(rc_vec2i_make(4, -3), rc_vec2i_make(4, 6))));
    RC_CHECK(rc_box2i_size(moved), ==, rc_box2i_size(box));
    RC_CHECK_TRUE(rc_box2i_is_equal(rc_box2i_translate(box, rc_vec2i_make_zero()), box));
}


RC_TEST(box2i, is_equal)
{
    rc_box2i box = rc_box2i_make(rc_vec2i_make(1, 2), rc_vec2i_make(4, 6));
    rc_box2i other_min = rc_box2i_make(rc_vec2i_make(0, 2), rc_vec2i_make(4, 6));
    rc_box2i other_max = rc_box2i_make(rc_vec2i_make(1, 2), rc_vec2i_make(4, 7));

    RC_CHECK_TRUE(rc_box2i_is_equal(box, rc_box2i_make_pos_size(rc_vec2i_make(1, 2), rc_vec2i_make(3, 4))));
    RC_CHECK_TRUE(rc_box2i_is_equal(box, box));
    RC_CHECK_FALSE(rc_box2i_is_equal(box, other_min));
    RC_CHECK_FALSE(rc_box2i_is_equal(box, other_max));
}
