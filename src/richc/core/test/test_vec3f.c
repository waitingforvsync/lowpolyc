// test_vec3f.c
//
// Tests for rc_vec3f.

#include "richc/math/vec3f.h"

#include "richc/test.h"


RC_TEST(vec3f, make)
{
    float components[] = {5.0f, 6.0f, 7.0f};
    rc_vec3f v = rc_vec3f_make(1.0f, 2.0f, 3.0f);

    RC_CHECK(v.x, ==, 1.0f);
    RC_CHECK(v.y, ==, 2.0f);
    RC_CHECK(v.z, ==, 3.0f);
    RC_CHECK(rc_vec3f_make_zero(), ==, rc_vec3f_make(0.0f, 0.0f, 0.0f));
    RC_CHECK(rc_vec3f_make_unitx(), ==, rc_vec3f_make(1.0f, 0.0f, 0.0f));
    RC_CHECK(rc_vec3f_make_unity(), ==, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(rc_vec3f_make_unitz(), ==, rc_vec3f_make(0.0f, 0.0f, 1.0f));
    RC_CHECK(rc_vec3f_from_floats(components), ==, rc_vec3f_make(5.0f, 6.0f, 7.0f));
    RC_CHECK(rc_vec3f_from_vec2f(rc_vec2f_make(1.0f, 2.0f), 3.0f), ==, rc_vec3f_make(1.0f, 2.0f, 3.0f));
    RC_CHECK(rc_vec3f_from_vec3i(rc_vec3i_make(7, -8, 9)), ==, rc_vec3f_make(7.0f, -8.0f, 9.0f));
    RC_CHECK_TRUE(rc_vec3f_as_floats(&v) == &v.x);
    RC_CHECK(rc_vec3f_as_floats(&v)[2], ==, 3.0f);
}


RC_TEST(vec3f, arithmetic)
{
    rc_vec3f a = rc_vec3f_make(1.0f, 2.0f, 3.0f);
    rc_vec3f b = rc_vec3f_make(10.0f, 20.0f, 30.0f);

    RC_CHECK(rc_vec3f_add(a, b), ==, rc_vec3f_make(11.0f, 22.0f, 33.0f));
    RC_CHECK(rc_vec3f_add3(a, b, a), ==, rc_vec3f_make(12.0f, 24.0f, 36.0f));
    RC_CHECK(rc_vec3f_add4(a, b, a, b), ==, rc_vec3f_make(22.0f, 44.0f, 66.0f));
    RC_CHECK(rc_vec3f_sub(b, a), ==, rc_vec3f_make(9.0f, 18.0f, 27.0f));
    RC_CHECK(rc_vec3f_scalar_mul(a, 3.0f), ==, rc_vec3f_make(3.0f, 6.0f, 9.0f));
    RC_CHECK(rc_vec3f_scalar_div(b, 10.0f), ==, rc_vec3f_make(1.0f, 2.0f, 3.0f));
    RC_CHECK(rc_vec3f_component_mul(a, b), ==, rc_vec3f_make(10.0f, 40.0f, 90.0f));
    RC_CHECK(rc_vec3f_negate(a), ==, rc_vec3f_make(-1.0f, -2.0f, -3.0f));
}


RC_TEST(vec3f, component_ops)
{
    rc_vec3f a = rc_vec3f_make(1.0f, 8.0f, 4.0f);
    rc_vec3f b = rc_vec3f_make(5.0f, 3.0f, 6.0f);
    rc_vec3f fractions = rc_vec3f_make(1.7f, -1.2f, 3.0f);

    RC_CHECK(rc_vec3f_component_min(a, b), ==, rc_vec3f_make(1.0f, 3.0f, 4.0f));
    RC_CHECK(rc_vec3f_component_max(a, b), ==, rc_vec3f_make(5.0f, 8.0f, 6.0f));
    RC_CHECK(rc_vec3f_component_floor(fractions), ==, rc_vec3f_make(1.0f, -2.0f, 3.0f));
    RC_CHECK(rc_vec3f_component_ceil(fractions), ==, rc_vec3f_make(2.0f, -1.0f, 3.0f));
    RC_CHECK(rc_vec3f_component_abs(rc_vec3f_make(-1.5f, 2.5f, -3.0f)), ==, rc_vec3f_make(1.5f, 2.5f, 3.0f));
}


RC_TEST(vec3f, lerp)
{
    rc_vec3f a = rc_vec3f_make(0.0f, 0.0f, 0.0f);
    rc_vec3f b = rc_vec3f_make(10.0f, 20.0f, 40.0f);

    RC_CHECK(rc_vec3f_lerp(a, b, 0.0f), ==, a);
    RC_CHECK(rc_vec3f_lerp(a, b, 1.0f), ==, b);
    RC_CHECK(rc_vec3f_lerp(a, b, 0.25f), ==, rc_vec3f_make(2.5f, 5.0f, 10.0f));
}


RC_TEST(vec3f, dot)
{
    rc_vec3f a = rc_vec3f_make(1.0f, 2.0f, 3.0f);
    rc_vec3f b = rc_vec3f_make(4.0f, 5.0f, 6.0f);

    RC_CHECK(rc_vec3f_dot(a, b), ==, 32.0f);
    RC_CHECK(rc_vec3f_dot(a, rc_vec3f_negate(b)), ==, -32.0f);
    RC_CHECK(rc_vec3f_dot(rc_vec3f_make_unitx(), rc_vec3f_make_unity()), ==, 0.0f);
}


RC_TEST(vec3f, cross)
{
    rc_vec3f a = rc_vec3f_make(1.0f, 2.0f, 3.0f);
    rc_vec3f b = rc_vec3f_make(4.0f, 5.0f, 6.0f);

    RC_CHECK(rc_vec3f_cross(rc_vec3f_make_unitx(), rc_vec3f_make_unity()), ==, rc_vec3f_make_unitz());
    RC_CHECK(rc_vec3f_cross(rc_vec3f_make_unity(), rc_vec3f_make_unitz()), ==, rc_vec3f_make_unitx());
    RC_CHECK(rc_vec3f_cross(rc_vec3f_make_unitz(), rc_vec3f_make_unitx()), ==, rc_vec3f_make_unity());
    RC_CHECK(rc_vec3f_cross(a, b), ==, rc_vec3f_make(-3.0f, 6.0f, -3.0f));
    RC_CHECK(rc_vec3f_cross(b, a), ==, rc_vec3f_make(3.0f, -6.0f, 3.0f));
    RC_CHECK(rc_vec3f_dot(a, rc_vec3f_cross(a, b)), ==, 0.0f);
    RC_CHECK(rc_vec3f_dot(b, rc_vec3f_cross(a, b)), ==, 0.0f);
}


RC_TEST(vec3f, length_normalize)
{
    rc_vec3f v = rc_vec3f_make(1.0f, 2.0f, 2.0f);

    RC_CHECK(rc_vec3f_lengthsqr(v), ==, 9.0f);
    RC_CHECK(rc_vec3f_length(v), ==, 3.0f);
    RC_CHECK(rc_vec3f_normalize(v), ==, rc_vec3f_make(1.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f));
    RC_CHECK(rc_vec3f_length(rc_vec3f_normalize(rc_vec3f_make(1.0f, 1.0f, 1.0f))), ~=, 1.0f);
}


RC_TEST(vec3f, normalize_safe)
{
    rc_vec3f v = rc_vec3f_make(1.0f, 2.0f, 2.0f);
    rc_vec3f unit = rc_vec3f_make(1.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f);

    RC_CHECK(rc_vec3f_normalize_safe(v, 0.001f), ==, unit);
    RC_CHECK(rc_vec3f_normalize_safe(rc_vec3f_make_zero(), 0.001f), ==, rc_vec3f_make_zero());

    // A length of exactly the tolerance is long enough
    RC_CHECK(rc_vec3f_normalize_safe(v, 3.0f), ==, unit);
    RC_CHECK(rc_vec3f_normalize_safe(v, 3.001f), ==, rc_vec3f_make_zero());
}


RC_TEST(vec3f, is_equal)
{
    rc_vec3f v = rc_vec3f_make(1.0f, 1.0f, 1.0f);

    RC_CHECK_TRUE(rc_vec3f_is_equal(v, rc_vec3f_make(1.0f, 1.0f, 1.0f)));
    RC_CHECK_FALSE(rc_vec3f_is_equal(v, rc_vec3f_make(2.0f, 1.0f, 1.0f)));
    RC_CHECK_FALSE(rc_vec3f_is_equal(v, rc_vec3f_make(1.0f, 2.0f, 1.0f)));
    RC_CHECK_FALSE(rc_vec3f_is_equal(v, rc_vec3f_make(1.0f, 1.0f, 2.0f)));
    RC_CHECK(v, ==, rc_vec3f_make(1.0f, 1.0f, 1.0f));
    RC_CHECK(v, !=, rc_vec3f_make(2.0f, 1.0f, 1.0f));
    RC_CHECK(v, ~=, rc_vec3f_make(1.00001f, 1.0f, 1.0f));
}


RC_TEST(vec3f, is_nearly_equal)
{
    rc_vec3f ones = rc_vec3f_make(1.0f, 1.0f, 1.0f);
    rc_vec3f origin = rc_vec3f_make_zero();
    rc_vec3f three_away = rc_vec3f_make(1.0f, 2.0f, 2.0f);

    RC_CHECK_TRUE(rc_vec3f_is_nearly_equal(ones, rc_vec3f_make(1.00001f, 1.0f, 1.0f), 0.001f));
    RC_CHECK_FALSE(rc_vec3f_is_nearly_equal(ones, rc_vec3f_make(2.0f, 1.0f, 1.0f), 0.001f));

    // The distance is Euclidean, and must be under the tolerance, not on it
    RC_CHECK_FALSE(rc_vec3f_is_nearly_equal(origin, three_away, 2.5f));
    RC_CHECK_FALSE(rc_vec3f_is_nearly_equal(origin, three_away, 3.0f));
    RC_CHECK_TRUE(rc_vec3f_is_nearly_equal(origin, three_away, 3.001f));
}
