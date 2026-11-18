// test_vec2f.c
//
// Tests for rc_vec2f.

#include "richc/math/vec2f.h"

#include "richc/test.h"


RC_TEST(vec2f, make)
{
    float components[] = {5.0f, 6.0f};
    rc_vec2f v = rc_vec2f_make(3.0f, 4.0f);

    RC_CHECK(v.x, ==, 3.0f);
    RC_CHECK(v.y, ==, 4.0f);
    RC_CHECK(rc_vec2f_make_zero(), ==, rc_vec2f_make(0.0f, 0.0f));
    RC_CHECK(rc_vec2f_make_unitx(), ==, rc_vec2f_make(1.0f, 0.0f));
    RC_CHECK(rc_vec2f_make_unity(), ==, rc_vec2f_make(0.0f, 1.0f));
    RC_CHECK(rc_vec2f_from_floats(components), ==, rc_vec2f_make(5.0f, 6.0f));
    RC_CHECK(rc_vec2f_from_vec2i(rc_vec2i_make(7, -8)), ==, rc_vec2f_make(7.0f, -8.0f));
    RC_CHECK_TRUE(rc_vec2f_as_floats(&v) == &v.x);
    RC_CHECK(rc_vec2f_as_floats(&v)[1], ==, 4.0f);
}


RC_TEST(vec2f, arithmetic)
{
    rc_vec2f a = rc_vec2f_make(1.0f, 2.0f);
    rc_vec2f b = rc_vec2f_make(10.0f, 20.0f);

    RC_CHECK(rc_vec2f_add(a, b), ==, rc_vec2f_make(11.0f, 22.0f));
    RC_CHECK(rc_vec2f_add3(a, b, a), ==, rc_vec2f_make(12.0f, 24.0f));
    RC_CHECK(rc_vec2f_add4(a, b, a, b), ==, rc_vec2f_make(22.0f, 44.0f));
    RC_CHECK(rc_vec2f_sub(b, a), ==, rc_vec2f_make(9.0f, 18.0f));
    RC_CHECK(rc_vec2f_scalar_mul(a, 3.0f), ==, rc_vec2f_make(3.0f, 6.0f));
    RC_CHECK(rc_vec2f_scalar_div(b, 5.0f), ==, rc_vec2f_make(2.0f, 4.0f));
    RC_CHECK(rc_vec2f_component_mul(a, b), ==, rc_vec2f_make(10.0f, 40.0f));
    RC_CHECK(rc_vec2f_negate(a), ==, rc_vec2f_make(-1.0f, -2.0f));
}


RC_TEST(vec2f, component_ops)
{
    rc_vec2f a = rc_vec2f_make(1.0f, 8.0f);
    rc_vec2f b = rc_vec2f_make(5.0f, 3.0f);
    rc_vec2f fractions = rc_vec2f_make(1.7f, -1.2f);

    RC_CHECK(rc_vec2f_component_min(a, b), ==, rc_vec2f_make(1.0f, 3.0f));
    RC_CHECK(rc_vec2f_component_max(a, b), ==, rc_vec2f_make(5.0f, 8.0f));
    RC_CHECK(rc_vec2f_component_floor(fractions), ==, rc_vec2f_make(1.0f, -2.0f));
    RC_CHECK(rc_vec2f_component_ceil(fractions), ==, rc_vec2f_make(2.0f, -1.0f));
    RC_CHECK(rc_vec2f_component_abs(rc_vec2f_make(-1.5f, 2.5f)), ==, rc_vec2f_make(1.5f, 2.5f));
}


RC_TEST(vec2f, lerp)
{
    rc_vec2f a = rc_vec2f_make(0.0f, 4.0f);
    rc_vec2f b = rc_vec2f_make(10.0f, -4.0f);

    RC_CHECK(rc_vec2f_lerp(a, b, 0.0f), ==, a);
    RC_CHECK(rc_vec2f_lerp(a, b, 1.0f), ==, b);
    RC_CHECK(rc_vec2f_lerp(a, b, 0.25f), ==, rc_vec2f_make(2.5f, 2.0f));
    RC_CHECK(rc_vec2f_lerp(a, b, 2.0f), ==, rc_vec2f_make(20.0f, -12.0f));
}


RC_TEST(vec2f, products)
{
    rc_vec2f a = rc_vec2f_make(1.0f, 2.0f);
    rc_vec2f b = rc_vec2f_make(3.0f, 4.0f);

    RC_CHECK(rc_vec2f_dot(a, b), ==, 11.0f);
    RC_CHECK(rc_vec2f_wedge(a, b), ==, -2.0f);
    RC_CHECK(rc_vec2f_wedge(b, a), ==, 2.0f);
}


RC_TEST(vec2f, perp)
{
    rc_vec2f v = rc_vec2f_make(2.0f, 3.0f);

    RC_CHECK(rc_vec2f_perp(v), ==, rc_vec2f_make(-3.0f, 2.0f));
    RC_CHECK(rc_vec2f_dot(v, rc_vec2f_perp(v)), ==, 0.0f);
    RC_CHECK(rc_vec2f_wedge(v, rc_vec2f_perp(v)), ==, rc_vec2f_lengthsqr(v));
}


RC_TEST(vec2f, length_normalize)
{
    rc_vec2f v = rc_vec2f_make(3.0f, 4.0f);

    RC_CHECK(rc_vec2f_lengthsqr(v), ==, 25.0f);
    RC_CHECK(rc_vec2f_length(v), ==, 5.0f);
    RC_CHECK(rc_vec2f_normalize(v), ==, rc_vec2f_make(0.6f, 0.8f));
    RC_CHECK(rc_vec2f_length(rc_vec2f_normalize(rc_vec2f_make(1.0f, 1.0f))), ~=, 1.0f);
}


RC_TEST(vec2f, normalize_safe)
{
    rc_vec2f v = rc_vec2f_make(3.0f, 4.0f);

    RC_CHECK(rc_vec2f_normalize_safe(v, 0.001f), ==, rc_vec2f_make(0.6f, 0.8f));
    RC_CHECK(rc_vec2f_normalize_safe(rc_vec2f_make_zero(), 0.001f), ==, rc_vec2f_make_zero());

    // A length of exactly the tolerance is long enough
    RC_CHECK(rc_vec2f_normalize_safe(v, 5.0f), ==, rc_vec2f_make(0.6f, 0.8f));
    RC_CHECK(rc_vec2f_normalize_safe(v, 5.001f), ==, rc_vec2f_make_zero());
}


RC_TEST(vec2f, is_equal)
{
    rc_vec2f v = rc_vec2f_make(1.0f, 1.0f);

    RC_CHECK_TRUE(rc_vec2f_is_equal(v, rc_vec2f_make(1.0f, 1.0f)));
    RC_CHECK_FALSE(rc_vec2f_is_equal(v, rc_vec2f_make(2.0f, 1.0f)));
    RC_CHECK_FALSE(rc_vec2f_is_equal(v, rc_vec2f_make(1.0f, 2.0f)));
    RC_CHECK_TRUE(rc_vec2f_is_equal(rc_vec2f_make(0.0f, 0.0f), rc_vec2f_make(-0.0f, -0.0f)));
    RC_CHECK(v, ==, rc_vec2f_make(1.0f, 1.0f));
    RC_CHECK(v, !=, rc_vec2f_make(2.0f, 1.0f));
    RC_CHECK(v, ~=, rc_vec2f_make(1.00001f, 1.0f));
}


RC_TEST(vec2f, is_nearly_equal)
{
    rc_vec2f ones = rc_vec2f_make(1.0f, 1.0f);
    rc_vec2f origin = rc_vec2f_make_zero();
    rc_vec2f five_away = rc_vec2f_make(3.0f, 4.0f);

    RC_CHECK_TRUE(rc_vec2f_is_nearly_equal(ones, rc_vec2f_make(1.00001f, 1.0f), 0.001f));
    RC_CHECK_FALSE(rc_vec2f_is_nearly_equal(ones, rc_vec2f_make(2.0f, 1.0f), 0.001f));

    // The distance is Euclidean, and must be under the tolerance, not on it
    RC_CHECK_FALSE(rc_vec2f_is_nearly_equal(origin, five_away, 4.5f));
    RC_CHECK_FALSE(rc_vec2f_is_nearly_equal(origin, five_away, 5.0f));
    RC_CHECK_TRUE(rc_vec2f_is_nearly_equal(origin, five_away, 5.001f));
}


RC_TEST(vec2f, make_sincos_and_cossin)
{
    rc_vec2f sincos = rc_vec2f_make_sincos(0.52359878f);
    rc_vec2f cossin = rc_vec2f_make_cossin(0.52359878f);

    // 30 degrees, whose sine is 1/2 and cosine sqrt(3) / 2
    RC_CHECK(sincos, ~=, rc_vec2f_make(0.5f, 0.8660254f));
    RC_CHECK(cossin, ~=, rc_vec2f_make(0.8660254f, 0.5f));

    // No turn is exact
    RC_CHECK(rc_vec2f_make_sincos(0.0f), ==, rc_vec2f_make_unity());
    RC_CHECK(rc_vec2f_make_cossin(0.0f), ==, rc_vec2f_make_unitx());
}
