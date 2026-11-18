// test_vec2i.c
//
// Tests for rc_vec2i.

#include "richc/math/vec2i.h"

#include "richc/test.h"


RC_TEST(vec2i, make)
{
    int32_t components[] = {5, 6};
    rc_vec2i v = rc_vec2i_make(3, 4);

    RC_CHECK(v.x, ==, 3);
    RC_CHECK(v.y, ==, 4);
    RC_CHECK(rc_vec2i_make_zero(), ==, rc_vec2i_make(0, 0));
    RC_CHECK(rc_vec2i_make_unitx(), ==, rc_vec2i_make(1, 0));
    RC_CHECK(rc_vec2i_make_unity(), ==, rc_vec2i_make(0, 1));
    RC_CHECK(rc_vec2i_from_i32s(components), ==, rc_vec2i_make(5, 6));
    RC_CHECK_TRUE(rc_vec2i_as_i32s(&v) == &v.x);
    RC_CHECK(rc_vec2i_as_i32s(&v)[1], ==, 4);
}


RC_TEST(vec2i, arithmetic)
{
    rc_vec2i a = rc_vec2i_make(1, 2);
    rc_vec2i b = rc_vec2i_make(10, 20);

    RC_CHECK(rc_vec2i_add(a, b), ==, rc_vec2i_make(11, 22));
    RC_CHECK(rc_vec2i_add3(a, b, a), ==, rc_vec2i_make(12, 24));
    RC_CHECK(rc_vec2i_add4(a, b, a, b), ==, rc_vec2i_make(22, 44));
    RC_CHECK(rc_vec2i_sub(a, b), ==, rc_vec2i_make(-9, -18));
    RC_CHECK(rc_vec2i_scalar_mul(a, 3), ==, rc_vec2i_make(3, 6));
    RC_CHECK(rc_vec2i_scalar_mul(a, -3), ==, rc_vec2i_make(-3, -6));
    RC_CHECK(rc_vec2i_scalar_div(b, 5), ==, rc_vec2i_make(2, 4));
    RC_CHECK(rc_vec2i_component_mul(a, b), ==, rc_vec2i_make(10, 40));
    RC_CHECK(rc_vec2i_negate(a), ==, rc_vec2i_make(-1, -2));
}


RC_TEST(vec2i, scalar_div_rounds_towards_zero)
{
    RC_CHECK(rc_vec2i_scalar_div(rc_vec2i_make(7, -7), 2), ==, rc_vec2i_make(3, -3));
    RC_CHECK(rc_vec2i_scalar_div(rc_vec2i_make(7, -7), -2), ==, rc_vec2i_make(-3, 3));
    RC_CHECK(rc_vec2i_scalar_div(rc_vec2i_make(1, -1), 2), ==, rc_vec2i_make(0, 0));
}


RC_TEST(vec2i, component_min_max)
{
    rc_vec2i a = rc_vec2i_make(1, 8);
    rc_vec2i b = rc_vec2i_make(5, -3);

    RC_CHECK(rc_vec2i_component_min(a, b), ==, rc_vec2i_make(1, -3));
    RC_CHECK(rc_vec2i_component_max(a, b), ==, rc_vec2i_make(5, 8));
    RC_CHECK(rc_vec2i_component_min(a, a), ==, a);
    RC_CHECK(rc_vec2i_component_max(a, a), ==, a);
}


RC_TEST(vec2i, products)
{
    rc_vec2i a = rc_vec2i_make(1, 2);
    rc_vec2i b = rc_vec2i_make(3, 4);

    RC_CHECK(rc_vec2i_dot(a, b), ==, 11);
    RC_CHECK(rc_vec2i_wedge(a, b), ==, -2);
    RC_CHECK(rc_vec2i_wedge(b, a), ==, 2);
    RC_CHECK(rc_vec2i_wedge(a, a), ==, 0);
    RC_CHECK(rc_vec2i_lengthsqr(b), ==, 25);
    RC_CHECK(rc_vec2i_lengthsqr(rc_vec2i_make_zero()), ==, 0);
}


RC_TEST(vec2i, products_at_the_limits)
{
    rc_vec2i extremes = rc_vec2i_make(INT32_MAX, INT32_MIN);
    rc_vec2i most_negative = rc_vec2i_make(INT32_MIN, INT32_MIN);

    // Every product here is near 2^62, and the lengthsqr and wedges come within 2^32 of INT64_MAX
    RC_CHECK(rc_vec2i_lengthsqr(extremes), ==, INT64_C(9223372032559808513));
    RC_CHECK(rc_vec2i_dot(extremes, most_negative), ==, INT64_C(2147483648));
    RC_CHECK(rc_vec2i_wedge(rc_vec2i_make(INT32_MIN, INT32_MAX), most_negative), ==, INT64_C(9223372034707292160));
    RC_CHECK(rc_vec2i_wedge(most_negative, rc_vec2i_make(INT32_MAX, INT32_MIN)), ==, INT64_C(9223372034707292160));
}


RC_TEST(vec2i, perp)
{
    rc_vec2i v = rc_vec2i_make(2, 3);

    RC_CHECK(rc_vec2i_perp(v), ==, rc_vec2i_make(-3, 2));
    RC_CHECK(rc_vec2i_dot(v, rc_vec2i_perp(v)), ==, 0);
    RC_CHECK(rc_vec2i_wedge(v, rc_vec2i_perp(v)), ==, rc_vec2i_lengthsqr(v));
    RC_CHECK(rc_vec2i_perp(rc_vec2i_perp(v)), ==, rc_vec2i_negate(v));
}


RC_TEST(vec2i, is_equal)
{
    rc_vec2i v = rc_vec2i_make(1, 2);

    RC_CHECK_TRUE(rc_vec2i_is_equal(v, rc_vec2i_make(1, 2)));
    RC_CHECK_FALSE(rc_vec2i_is_equal(v, rc_vec2i_make(0, 2)));
    RC_CHECK_FALSE(rc_vec2i_is_equal(v, rc_vec2i_make(1, 3)));
    RC_CHECK(v, !=, rc_vec2i_make(2, 1));
}
