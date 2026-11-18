// test_vec3i.c
//
// Tests for rc_vec3i.

#include "richc/math/vec3i.h"

#include "richc/test.h"


RC_TEST(vec3i, make)
{
    int32_t components[] = {7, 8, 9};
    rc_vec3i v = rc_vec3i_make(1, 2, 3);

    RC_CHECK(v.x, ==, 1);
    RC_CHECK(v.y, ==, 2);
    RC_CHECK(v.z, ==, 3);
    RC_CHECK(rc_vec3i_make_zero(), ==, rc_vec3i_make(0, 0, 0));
    RC_CHECK(rc_vec3i_make_unitx(), ==, rc_vec3i_make(1, 0, 0));
    RC_CHECK(rc_vec3i_make_unity(), ==, rc_vec3i_make(0, 1, 0));
    RC_CHECK(rc_vec3i_make_unitz(), ==, rc_vec3i_make(0, 0, 1));
    RC_CHECK(rc_vec3i_from_i32s(components), ==, rc_vec3i_make(7, 8, 9));
    RC_CHECK(rc_vec3i_from_vec2i(rc_vec2i_make(4, 5), 6), ==, rc_vec3i_make(4, 5, 6));
    RC_CHECK_TRUE(rc_vec3i_as_i32s(&v) == &v.x);
    RC_CHECK(rc_vec3i_as_i32s(&v)[2], ==, 3);
}


RC_TEST(vec3i, arithmetic)
{
    rc_vec3i a = rc_vec3i_make(1, 2, 3);
    rc_vec3i b = rc_vec3i_make(10, 20, 30);

    RC_CHECK(rc_vec3i_add(a, b), ==, rc_vec3i_make(11, 22, 33));
    RC_CHECK(rc_vec3i_add3(a, b, a), ==, rc_vec3i_make(12, 24, 36));
    RC_CHECK(rc_vec3i_add4(a, b, a, b), ==, rc_vec3i_make(22, 44, 66));
    RC_CHECK(rc_vec3i_sub(b, a), ==, rc_vec3i_make(9, 18, 27));
    RC_CHECK(rc_vec3i_scalar_mul(a, 2), ==, rc_vec3i_make(2, 4, 6));
    RC_CHECK(rc_vec3i_scalar_div(b, 10), ==, rc_vec3i_make(1, 2, 3));
    RC_CHECK(rc_vec3i_scalar_div(rc_vec3i_make(7, -7, 1), 2), ==, rc_vec3i_make(3, -3, 0));
    RC_CHECK(rc_vec3i_component_mul(a, b), ==, rc_vec3i_make(10, 40, 90));
    RC_CHECK(rc_vec3i_negate(a), ==, rc_vec3i_make(-1, -2, -3));
}


RC_TEST(vec3i, component_min_max)
{
    rc_vec3i a = rc_vec3i_make(1, 2, 3);
    rc_vec3i b = rc_vec3i_make(0, 5, -1);

    RC_CHECK(rc_vec3i_component_min(a, b), ==, rc_vec3i_make(0, 2, -1));
    RC_CHECK(rc_vec3i_component_max(a, b), ==, rc_vec3i_make(1, 5, 3));
}


RC_TEST(vec3i, products)
{
    rc_vec3i a = rc_vec3i_make(1, 2, 3);
    rc_vec3i b = rc_vec3i_make(4, 5, 6);

    RC_CHECK(rc_vec3i_dot(a, b), ==, 32);
    RC_CHECK(rc_vec3i_lengthsqr(rc_vec3i_make(1, 2, 2)), ==, 9);
    RC_CHECK(rc_vec3i_cross(a, b), ==, rc_vec3i_make(-3, 6, -3));
    RC_CHECK(rc_vec3i_cross(b, a), ==, rc_vec3i_make(3, -6, 3));
    RC_CHECK(rc_vec3i_cross(a, a), ==, rc_vec3i_make_zero());
    RC_CHECK(rc_vec3i_dot(a, rc_vec3i_cross(a, b)), ==, 0);
}


RC_TEST(vec3i, cross_is_right_handed)
{
    rc_vec3i x = rc_vec3i_make_unitx();
    rc_vec3i y = rc_vec3i_make_unity();
    rc_vec3i z = rc_vec3i_make_unitz();

    RC_CHECK(rc_vec3i_cross(x, y), ==, z);
    RC_CHECK(rc_vec3i_cross(y, z), ==, x);
    RC_CHECK(rc_vec3i_cross(z, x), ==, y);
    RC_CHECK(rc_vec3i_cross(y, x), ==, rc_vec3i_negate(z));
}


RC_TEST(vec3i, cross_of_large_components)
{
    rc_vec3i a = rc_vec3i_make(100000, 100000, 1);
    rc_vec3i b = rc_vec3i_make(100000, 100001, 1);
    rc_vec3i most_negative = rc_vec3i_make(INT32_MIN, INT32_MIN, INT32_MIN);
    rc_vec3i most_positive = rc_vec3i_make(INT32_MAX, INT32_MAX, 0);
    rc_vec3i nearly_most_positive = rc_vec3i_make(INT32_MAX, INT32_MAX - 1, 0);

    // The products are around 10^10, but they cancel down to a result that fits
    RC_CHECK(rc_vec3i_cross(a, b), ==, rc_vec3i_make(-1, 0, 100000));

    // Products of 2^62 cancel exactly, and a result can land on either limit of an int32_t
    RC_CHECK(rc_vec3i_cross(most_negative, most_negative), ==, rc_vec3i_make_zero());
    RC_CHECK(rc_vec3i_cross(rc_vec3i_make(INT32_MIN, 0, 0), rc_vec3i_make_unity()), ==, rc_vec3i_make(0, 0, INT32_MIN));
    RC_CHECK(rc_vec3i_cross(rc_vec3i_make_unitx(), rc_vec3i_make(0, INT32_MAX, 0)), ==, rc_vec3i_make(0, 0, INT32_MAX));
    RC_CHECK(rc_vec3i_cross(most_positive, nearly_most_positive), ==, rc_vec3i_make(0, 0, -INT32_MAX));
}


RC_TEST(vec3i, products_at_the_limits)
{
    rc_vec3i extremes = rc_vec3i_make(INT32_MAX, INT32_MIN, 0);

    RC_CHECK(rc_vec3i_lengthsqr(extremes), ==, INT64_C(9223372032559808513));
    RC_CHECK(rc_vec3i_dot(extremes, rc_vec3i_make(INT32_MIN, INT32_MIN, INT32_MIN)), ==, INT64_C(2147483648));
}


RC_TEST(vec3i, is_equal)
{
    rc_vec3i v = rc_vec3i_make(1, 2, 3);

    RC_CHECK_TRUE(rc_vec3i_is_equal(v, rc_vec3i_make(1, 2, 3)));
    RC_CHECK_FALSE(rc_vec3i_is_equal(v, rc_vec3i_make(0, 2, 3)));
    RC_CHECK_FALSE(rc_vec3i_is_equal(v, rc_vec3i_make(1, 0, 3)));
    RC_CHECK_FALSE(rc_vec3i_is_equal(v, rc_vec3i_make(1, 2, 0)));
    RC_CHECK(v, !=, rc_vec3i_make(3, 2, 1));
}
