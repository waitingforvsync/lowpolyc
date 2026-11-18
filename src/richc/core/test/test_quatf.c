// test_quatf.c
//
// Tests for rc_quatf. The rotations are built from exact components: a quarter turn about an axis
// is the normalised (axis, 1).

#include "richc/math/quatf.h"

#include "richc/test.h"

// The sine and cosine of 22.5 degrees, for an eighth turn.
static const float sin_22_5 = 0.38268343f;
static const float cos_22_5 = 0.92387953f;


// The quarter turn about the unit axis.
static rc_quatf quarter_turn(rc_vec3f axis)
{
    return rc_quatf_normalize(rc_quatf_from_vec3f(axis, 1.0f));
}


// The third of a turn about (1, 1, 1), which takes x to y, y to z and z to x.
static rc_quatf third_turn(void)
{
    return rc_quatf_make(0.5f, 0.5f, 0.5f, 0.5f);
}


// Euler angles of 30, 40 and 50 degrees about x, y and z, as worked out by scipy.
static rc_quatf euler_30_40_50(void)
{
    return rc_quatf_make(0.0808047f, 0.4021985f, 0.3033718f, 0.8600422f);
}


// The same rotation as a matrix, also from scipy.
static rc_mat33f euler_30_40_50_matrix(void)
{
    return rc_mat33f_make(
        rc_vec3f_make(0.492404f, 0.586824f, -0.642788f),
        rc_vec3f_make(-0.456826f, 0.802872f, 0.383022f),
        rc_vec3f_make(0.740843f, 0.10504f, 0.663414f)
    );
}


RC_TEST(quatf, make)
{
    float components[] = {1.0f, 2.0f, 3.0f, 4.0f};
    rc_quatf q = rc_quatf_make(1.0f, 2.0f, 3.0f, 4.0f);
    rc_quatf identity = rc_quatf_make_identity();
    rc_quatf from_floats = rc_quatf_from_floats(components);
    rc_quatf from_vec3f = rc_quatf_from_vec3f(rc_vec3f_make(5.0f, 6.0f, 7.0f), 8.0f);

    RC_CHECK(q.xyz, ==, rc_vec3f_make(1.0f, 2.0f, 3.0f));
    RC_CHECK(q.w, ==, 4.0f);
    RC_CHECK(identity.xyz, ==, rc_vec3f_make_zero());
    RC_CHECK(identity.w, ==, 1.0f);
    RC_CHECK(from_floats.xyz, ==, rc_vec3f_make(1.0f, 2.0f, 3.0f));
    RC_CHECK(from_floats.w, ==, 4.0f);
    RC_CHECK(from_vec3f.xyz, ==, rc_vec3f_make(5.0f, 6.0f, 7.0f));
    RC_CHECK(from_vec3f.w, ==, 8.0f);
    RC_CHECK_TRUE(rc_quatf_as_floats(&q) == &q.xyz.x);
    RC_CHECK(rc_quatf_as_floats(&q)[3], ==, 4.0f);
}


RC_TEST(quatf, arithmetic)
{
    rc_quatf a = rc_quatf_make(1.0f, 2.0f, 3.0f, 4.0f);
    rc_quatf b = rc_quatf_make(10.0f, 20.0f, 30.0f, 40.0f);
    rc_quatf sum = rc_quatf_add(a, b);
    rc_quatf difference = rc_quatf_sub(b, a);
    rc_quatf scaled = rc_quatf_scalar_mul(a, 2.0f);
    rc_quatf negated = rc_quatf_negate(a);

    RC_CHECK(sum.xyz, ==, rc_vec3f_make(11.0f, 22.0f, 33.0f));
    RC_CHECK(sum.w, ==, 44.0f);
    RC_CHECK(difference.xyz, ==, rc_vec3f_make(9.0f, 18.0f, 27.0f));
    RC_CHECK(difference.w, ==, 36.0f);
    RC_CHECK(scaled.xyz, ==, rc_vec3f_make(2.0f, 4.0f, 6.0f));
    RC_CHECK(scaled.w, ==, 8.0f);
    RC_CHECK(negated.xyz, ==, rc_vec3f_make(-1.0f, -2.0f, -3.0f));
    RC_CHECK(negated.w, ==, -4.0f);
}


RC_TEST(quatf, lengths)
{
    rc_quatf a = rc_quatf_make(1.0f, 2.0f, 3.0f, 4.0f);
    rc_quatf b = rc_quatf_make(10.0f, 20.0f, 30.0f, 40.0f);
    rc_quatf normalized = rc_quatf_normalize(rc_quatf_make(1.0f, 2.0f, 2.0f, 4.0f));

    RC_CHECK(rc_quatf_dot(a, b), ==, 300.0f);
    RC_CHECK(rc_quatf_lengthsqr(a), ==, 30.0f);
    RC_CHECK(rc_quatf_length(a), ~=, 5.4772256f);
    RC_CHECK(rc_quatf_length(rc_quatf_normalize(a)), ~=, 1.0f);
    RC_CHECK(normalized.xyz, ~=, rc_vec3f_make(0.2f, 0.4f, 0.4f));
    RC_CHECK(normalized.w, ~=, 0.8f);
}


RC_TEST(quatf, conjugate_and_inverse)
{
    rc_quatf q = rc_quatf_make(1.0f, 2.0f, 3.0f, 4.0f);
    rc_quatf conjugate = rc_quatf_conjugate(q);
    rc_quatf times_inverse = rc_quatf_mul(q, rc_quatf_inverse(q));
    rc_quatf unit = quarter_turn(rc_vec3f_make_unitz());
    rc_quatf unit_inverse = rc_quatf_inverse(unit);
    rc_quatf unit_conjugate = rc_quatf_conjugate(unit);

    RC_CHECK(conjugate.xyz, ==, rc_vec3f_make(-1.0f, -2.0f, -3.0f));
    RC_CHECK(conjugate.w, ==, 4.0f);

    // Even for q not of unit length
    RC_CHECK(times_inverse.xyz, ~=, rc_vec3f_make_zero());
    RC_CHECK(times_inverse.w, ~=, 1.0f);

    // For a unit quaternion the inverse is the conjugate
    RC_CHECK(unit_inverse.xyz, ~=, unit_conjugate.xyz);
    RC_CHECK(unit_inverse.w, ~=, unit_conjugate.w);
}


RC_TEST(quatf, mul)
{
    rc_quatf quarter_z = quarter_turn(rc_vec3f_make_unitz());
    rc_quatf quarter_x = quarter_turn(rc_vec3f_make_unitx());
    rc_quatf quarter_y = quarter_turn(rc_vec3f_make_unity());
    rc_quatf by_identity = rc_quatf_mul(rc_quatf_make_identity(), quarter_z);
    rc_quatf undone = rc_quatf_mul(quarter_z, rc_quatf_conjugate(quarter_z));
    rc_quatf half_z = rc_quatf_mul(quarter_z, quarter_z);
    rc_quatf x_then_y = rc_quatf_mul(quarter_y, quarter_x);
    rc_vec3f v = rc_vec3f_make(0.0f, 1.0f, 0.0f);

    RC_CHECK(by_identity.xyz, ==, quarter_z.xyz);
    RC_CHECK(by_identity.w, ==, quarter_z.w);
    RC_CHECK(undone.xyz, ~=, rc_vec3f_make_zero());
    RC_CHECK(undone.w, ~=, 1.0f);
    RC_CHECK(half_z.xyz, ~=, rc_vec3f_make_unitz());
    RC_CHECK(half_z.w, ~=, 0.0f);

    // A quarter turn about x, then one about y
    RC_CHECK(x_then_y.xyz, ~=, rc_vec3f_make(0.5f, 0.5f, -0.5f));
    RC_CHECK(x_then_y.w, ~=, 0.5f);
    RC_CHECK(rc_quatf_vec3f_transform(x_then_y, v), ~=, rc_quatf_vec3f_transform(quarter_y, rc_quatf_vec3f_transform(quarter_x, v)));
}


RC_TEST(quatf, vec3f_transform)
{
    rc_quatf quarter_z = quarter_turn(rc_vec3f_make_unitz());
    rc_quatf third = third_turn();
    rc_vec3f v = rc_vec3f_make(2.0f, -1.0f, 3.0f);

    // Right-handed: a quarter turn about z sends +x to +y, and leaves the axis be
    RC_CHECK(rc_quatf_vec3f_transform(quarter_z, rc_vec3f_make_unitx()), ~=, rc_vec3f_make_unity());
    RC_CHECK(rc_quatf_vec3f_transform(quarter_z, rc_vec3f_make_unity()), ~=, rc_vec3f_make(-1.0f, 0.0f, 0.0f));
    RC_CHECK(rc_quatf_vec3f_transform(quarter_z, rc_vec3f_make_unitz()), ~=, rc_vec3f_make_unitz());
    RC_CHECK(rc_quatf_vec3f_transform(rc_quatf_make_identity(), v), ==, v);

    // The third of a turn cycles the axes, so its components are exact
    RC_CHECK(rc_quatf_vec3f_transform(third, rc_vec3f_make_unitx()), ==, rc_vec3f_make_unity());
    RC_CHECK(rc_quatf_vec3f_transform(third, rc_vec3f_make_unity()), ==, rc_vec3f_make_unitz());
    RC_CHECK(rc_quatf_vec3f_transform(third, v), ==, rc_vec3f_make(3.0f, 2.0f, -1.0f));
    RC_CHECK(rc_vec3f_length(rc_quatf_vec3f_transform(third, rc_vec3f_make(3.0f, 4.0f, 0.0f))), ==, 5.0f);

    // The two cross products agree with the matrix
    RC_CHECK(rc_quatf_vec3f_transform(third, v), ~=, rc_mat33f_vec3f_mul(rc_mat33f_from_quatf(third), v));
    RC_CHECK(rc_quatf_vec3f_transform(quarter_z, v), ~=, rc_mat33f_vec3f_mul(rc_mat33f_from_quatf(quarter_z), v));
}


RC_TEST(quatf, axis)
{
    rc_quatf tilted = rc_quatf_normalize(rc_quatf_make(1.0f, 2.0f, 2.0f, 4.0f));

    RC_CHECK(rc_quatf_axis(quarter_turn(rc_vec3f_make_unitz())), ~=, rc_vec3f_make_unitz());
    RC_CHECK(rc_quatf_axis(third_turn()), ~=, rc_vec3f_make(0.57735027f, 0.57735027f, 0.57735027f));
    RC_CHECK(rc_quatf_axis(tilted), ~=, rc_vec3f_make(1.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f));

    // A turn the other way about the opposite axis is the same rotation
    RC_CHECK(rc_quatf_axis(rc_quatf_negate(tilted)), ~=, rc_vec3f_make(-1.0f / 3.0f, -2.0f / 3.0f, -2.0f / 3.0f));

    // No turn has no axis, so it falls back to x
    RC_CHECK(rc_quatf_axis(rc_quatf_make_identity()), ==, rc_vec3f_make_unitx());
    RC_CHECK(rc_quatf_axis(rc_quatf_make(1e-7f, 0.0f, 0.0f, 1.0f)), ==, rc_vec3f_make_unitx());
}


RC_TEST(quatf, lerp_and_nlerp)
{
    rc_quatf identity = rc_quatf_make_identity();
    rc_quatf quarter_z = quarter_turn(rc_vec3f_make_unitz());
    rc_quatf halfway = rc_quatf_lerp(identity, quarter_z, 0.5f);
    rc_quatf eighth = rc_quatf_nlerp(identity, quarter_z, 0.5f);
    rc_quatf at_end = rc_quatf_nlerp(identity, quarter_z, 1.0f);

    // The lerp is a straight line, so its middle is shorter than 1
    RC_CHECK(halfway.xyz, ~=, rc_vec3f_make(0.0f, 0.0f, 0.35355339f));
    RC_CHECK(halfway.w, ~=, 0.85355339f);
    RC_CHECK(rc_quatf_length(halfway), <, 1.0f);

    // At the symmetric middle, the nlerp turns exactly half way
    RC_CHECK(eighth.xyz, ~=, rc_vec3f_make(0.0f, 0.0f, sin_22_5));
    RC_CHECK(eighth.w, ~=, cos_22_5);
    RC_CHECK(rc_quatf_length(eighth), ~=, 1.0f);
    RC_CHECK(at_end.xyz, ~=, quarter_z.xyz);
    RC_CHECK(at_end.w, ~=, quarter_z.w);
}


RC_TEST(quatf, to_mat33f)
{
    rc_mat33f quarter_z = rc_mat33f_from_quatf(quarter_turn(rc_vec3f_make_unitz()));
    rc_mat33f identity = rc_mat33f_from_quatf(rc_quatf_make_identity());
    rc_mat33f third = rc_mat33f_from_quatf(third_turn());
    rc_mat33f general = rc_mat33f_from_quatf(euler_30_40_50());
    rc_mat33f expected = euler_30_40_50_matrix();

    RC_CHECK(quarter_z.cx, ~=, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(quarter_z.cy, ~=, rc_vec3f_make(-1.0f, 0.0f, 0.0f));
    RC_CHECK(quarter_z.cz, ~=, rc_vec3f_make(0.0f, 0.0f, 1.0f));
    RC_CHECK(identity.cx, ==, rc_vec3f_make_unitx());
    RC_CHECK(identity.cy, ==, rc_vec3f_make_unity());
    RC_CHECK(identity.cz, ==, rc_vec3f_make_unitz());
    RC_CHECK(third.cx, ==, rc_vec3f_make_unity());
    RC_CHECK(third.cy, ==, rc_vec3f_make_unitz());
    RC_CHECK(third.cz, ==, rc_vec3f_make_unitx());
    RC_CHECK(general.cx, ~=, expected.cx);
    RC_CHECK(general.cy, ~=, expected.cy);
    RC_CHECK(general.cz, ~=, expected.cz);
}


// ---- from a matrix: one test for each component that can be the largest ----

RC_TEST(quatf, from_mat33f_w_largest)
{
    rc_quatf q = rc_quatf_make(0.4f, 0.2f, 0.4f, 0.8f);
    rc_mat33f m = rc_mat33f_from_quatf(q);
    rc_quatf back = rc_quatf_from_mat33f(m);
    rc_mat33f m_back = rc_mat33f_from_quatf(back);

    RC_CHECK(back.xyz, ~=, q.xyz);
    RC_CHECK(back.w, ~=, q.w);
    RC_CHECK(m_back.cx, ~=, m.cx);
    RC_CHECK(m_back.cy, ~=, m.cy);
    RC_CHECK(m_back.cz, ~=, m.cz);
}


RC_TEST(quatf, from_mat33f_x_largest)
{
    rc_quatf q = rc_quatf_make(0.8f, 0.4f, 0.2f, 0.4f);
    rc_mat33f m = rc_mat33f_from_quatf(q);
    rc_quatf back = rc_quatf_from_mat33f(m);
    rc_mat33f m_back = rc_mat33f_from_quatf(back);

    RC_CHECK(back.xyz, ~=, q.xyz);
    RC_CHECK(back.w, ~=, q.w);
    RC_CHECK(m_back.cx, ~=, m.cx);
    RC_CHECK(m_back.cy, ~=, m.cy);
    RC_CHECK(m_back.cz, ~=, m.cz);
}


RC_TEST(quatf, from_mat33f_y_largest)
{
    rc_quatf q = rc_quatf_make(0.4f, 0.8f, 0.2f, 0.4f);
    rc_mat33f m = rc_mat33f_from_quatf(q);
    rc_quatf back = rc_quatf_from_mat33f(m);
    rc_mat33f m_back = rc_mat33f_from_quatf(back);

    RC_CHECK(back.xyz, ~=, q.xyz);
    RC_CHECK(back.w, ~=, q.w);
    RC_CHECK(m_back.cx, ~=, m.cx);
    RC_CHECK(m_back.cy, ~=, m.cy);
    RC_CHECK(m_back.cz, ~=, m.cz);
}


RC_TEST(quatf, from_mat33f_z_largest)
{
    rc_quatf q = rc_quatf_make(0.4f, 0.2f, 0.8f, 0.4f);
    rc_mat33f m = rc_mat33f_from_quatf(q);
    rc_quatf back = rc_quatf_from_mat33f(m);
    rc_mat33f m_back = rc_mat33f_from_quatf(back);

    RC_CHECK(back.xyz, ~=, q.xyz);
    RC_CHECK(back.w, ~=, q.w);
    RC_CHECK(m_back.cx, ~=, m.cx);
    RC_CHECK(m_back.cy, ~=, m.cy);
    RC_CHECK(m_back.cz, ~=, m.cz);
}


RC_TEST(quatf, from_mat33f_half_turns)
{
    rc_mat33f half_x = rc_mat33f_make(rc_vec3f_make(1.0f, 0.0f, 0.0f), rc_vec3f_make(0.0f, -1.0f, 0.0f), rc_vec3f_make(0.0f, 0.0f, -1.0f));
    rc_mat33f half_y = rc_mat33f_make(rc_vec3f_make(-1.0f, 0.0f, 0.0f), rc_vec3f_make(0.0f, 1.0f, 0.0f), rc_vec3f_make(0.0f, 0.0f, -1.0f));
    rc_mat33f half_z = rc_mat33f_make(rc_vec3f_make(-1.0f, 0.0f, 0.0f), rc_vec3f_make(0.0f, -1.0f, 0.0f), rc_vec3f_make(0.0f, 0.0f, 1.0f));
    rc_quatf from_half_x = rc_quatf_from_mat33f(half_x);
    rc_quatf from_half_y = rc_quatf_from_mat33f(half_y);
    rc_quatf from_half_z = rc_quatf_from_mat33f(half_z);

    // w is 0 in each, which is where taking each component from its own square root loses the signs
    RC_CHECK(from_half_x.xyz, ==, rc_vec3f_make_unitx());
    RC_CHECK(from_half_x.w, ==, 0.0f);
    RC_CHECK(from_half_y.xyz, ==, rc_vec3f_make_unity());
    RC_CHECK(from_half_y.w, ==, 0.0f);
    RC_CHECK(from_half_z.xyz, ==, rc_vec3f_make_unitz());
    RC_CHECK(from_half_z.w, ==, 0.0f);
}


RC_TEST(quatf, from_mat33f_known_turns)
{
    rc_mat33f quarter_z = rc_mat33f_make(rc_vec3f_make(0.0f, 1.0f, 0.0f), rc_vec3f_make(-1.0f, 0.0f, 0.0f), rc_vec3f_make_unitz());
    rc_quatf from_quarter_z = rc_quatf_from_mat33f(quarter_z);
    rc_quatf from_identity = rc_quatf_from_mat33f(rc_mat33f_make_identity());
    rc_quatf from_third = rc_quatf_from_mat33f(rc_mat33f_from_quatf(third_turn()));
    rc_quatf from_general = rc_quatf_from_mat33f(euler_30_40_50_matrix());

    RC_CHECK(from_quarter_z.xyz, ~=, rc_vec3f_make(0.0f, 0.0f, 0.70710678f));
    RC_CHECK(from_quarter_z.w, ~=, 0.70710678f);
    RC_CHECK(from_identity.xyz, ==, rc_vec3f_make_zero());
    RC_CHECK(from_identity.w, ==, 1.0f);
    RC_CHECK(from_third.xyz, ==, third_turn().xyz);
    RC_CHECK(from_third.w, ==, third_turn().w);
    RC_CHECK(from_general.xyz, ~=, euler_30_40_50().xyz);
    RC_CHECK(from_general.w, ~=, euler_30_40_50().w);
}


RC_TEST(quatf, is_equal)
{
    rc_quatf q = rc_quatf_make(1.0f, 2.0f, 3.0f, 4.0f);

    RC_CHECK_TRUE(rc_quatf_is_equal(q, rc_quatf_make(1.0f, 2.0f, 3.0f, 4.0f)));
    RC_CHECK_FALSE(rc_quatf_is_equal(q, rc_quatf_make(1.0f, 2.0f, 3.0f, 5.0f)));
    RC_CHECK_FALSE(rc_quatf_is_equal(q, rc_quatf_make(1.0f, 2.0f, 0.0f, 4.0f)));

    // -q is the same rotation, but not the same quaternion
    RC_CHECK_FALSE(rc_quatf_is_equal(q, rc_quatf_negate(q)));
    RC_CHECK_TRUE(rc_quatf_is_nearly_equal(q, rc_quatf_make(1.00001f, 2.0f, 3.0f, 4.0f), 0.001f));
    RC_CHECK_FALSE(rc_quatf_is_nearly_equal(q, rc_quatf_make(1.0f, 2.0f, 3.0f, 5.0f), 0.001f));
    RC_CHECK_FALSE(rc_quatf_is_nearly_equal(q, rc_quatf_make(1.0f, 2.0f, 3.0f, 5.0f), 1.0f));
    RC_CHECK_TRUE(rc_quatf_is_nearly_equal(q, rc_quatf_make(1.0f, 2.0f, 3.0f, 5.0f), 1.001f));
}


RC_TEST(quatf, make_angle_axis)
{
    rc_quatf quarter = rc_quatf_make_angle_axis(1.5707963f, rc_vec3f_make_unitz());
    rc_quatf third = rc_quatf_make_angle_axis(2.0943951f, rc_vec3f_make(1.0f, 1.0f, 1.0f));
    rc_quatf long_axis = rc_quatf_make_angle_axis(1.5707963f, rc_vec3f_make(0.0f, 0.0f, 5.0f));
    rc_quatf none = rc_quatf_make_angle_axis(0.0f, rc_vec3f_make_unity());

    // A quarter turn about z is (0, 0, sin 45, cos 45)
    RC_CHECK(quarter.xyz, ~=, rc_vec3f_make(0.0f, 0.0f, 0.70710678f));
    RC_CHECK(quarter.w, ~=, 0.70710678f);
    RC_CHECK(rc_quatf_length(quarter), ~=, 1.0f);
    RC_CHECK_TRUE(rc_quatf_is_nearly_equal(quarter, quarter_turn(rc_vec3f_make_unitz()), 1e-6f));

    // A third of a turn about (1, 1, 1) is a half in every component
    RC_CHECK_TRUE(rc_quatf_is_nearly_equal(third, third_turn(), 1e-6f));

    // The axis is normalised, so its length doesn't matter
    RC_CHECK_TRUE(rc_quatf_is_nearly_equal(long_axis, quarter, 1e-6f));

    // No turn is exactly the identity
    RC_CHECK_TRUE(rc_quatf_is_equal(none, rc_quatf_make_identity()));
}


RC_TEST(quatf, angle)
{
    rc_quatf quarter = quarter_turn(rc_vec3f_make_unitz());

    RC_CHECK(rc_quatf_angle(quarter), ~=, 1.5707963f);
    RC_CHECK(rc_quatf_angle(third_turn()), ~=, 2.0943951f);
    RC_CHECK(rc_quatf_angle(rc_quatf_make_identity()), ==, 0.0f);

    // A half turn has no scalar part
    RC_CHECK(rc_quatf_angle(rc_quatf_make(0.0f, 1.0f, 0.0f, 0.0f)), ~=, 3.1415927f);
}


RC_TEST(quatf, slerp)
{
    rc_quatf identity = rc_quatf_make_identity();
    rc_quatf quarter = quarter_turn(rc_vec3f_make_unitz());
    rc_quatf start = rc_quatf_slerp(identity, quarter, 0.0f);
    rc_quatf end = rc_quatf_slerp(identity, quarter, 1.0f);
    rc_quatf middle = rc_quatf_slerp(identity, quarter, 0.5f);
    rc_quatf middle_negated = rc_quatf_slerp(identity, rc_quatf_negate(quarter), 0.5f);
    rc_quatf nearly_same = rc_quatf_slerp(identity, rc_quatf_normalize(rc_quatf_make(0.0f, 0.0f, 0.01f, 1.0f)), 0.5f);

    RC_CHECK_TRUE(rc_quatf_is_nearly_equal(start, identity, 1e-6f));
    RC_CHECK_TRUE(rc_quatf_is_nearly_equal(end, quarter, 1e-6f));

    // Halfway to a quarter turn is an eighth of a turn
    RC_CHECK(middle.xyz, ~=, rc_vec3f_make(0.0f, 0.0f, sin_22_5));
    RC_CHECK(middle.w, ~=, cos_22_5);

    // -q is the same rotation, and slerp takes the short way round to it
    RC_CHECK(middle_negated.xyz, ~=, middle.xyz);
    RC_CHECK(middle_negated.w, ~=, middle.w);

    // Nearly the same rotations fall back to nlerp, which still lands halfway
    RC_CHECK(nearly_same.xyz.z, ~=, 0.005f);
    RC_CHECK(rc_quatf_length(nearly_same), ~=, 1.0f);
}


RC_TEST(quatf, exp_log_pow)
{
    rc_quatf quarter = quarter_turn(rc_vec3f_make_unitz());
    rc_quatf logarithm = rc_quatf_log(quarter);
    rc_quatf exponential = rc_quatf_exp(rc_quatf_make(0.0f, 0.0f, 0.78539816f, 0.0f));
    rc_quatf round_trip = rc_quatf_exp(rc_quatf_log(quarter));
    rc_quatf half = rc_quatf_pow(quarter, 0.5f);
    rc_quatf squared = rc_quatf_pow(quarter, 2.0f);
    rc_quatf log_of_real = rc_quatf_log(rc_quatf_make(0.0f, 0.0f, 0.0f, 2.0f));

    // The logarithm of a unit rotation is (axis angle / 2, 0)
    RC_CHECK(logarithm.xyz, ~=, rc_vec3f_make(0.0f, 0.0f, 0.78539816f));
    RC_CHECK(logarithm.w, ~=, 0.0f);

    // The exponential of a pure quaternion is the rotation it generates, and undoes the log
    RC_CHECK_TRUE(rc_quatf_is_nearly_equal(exponential, quarter, 1e-6f));
    RC_CHECK_TRUE(rc_quatf_is_nearly_equal(round_trip, quarter, 1e-6f));

    // A half power is half the turn, and squaring doubles it
    RC_CHECK(half.xyz, ~=, rc_vec3f_make(0.0f, 0.0f, sin_22_5));
    RC_CHECK(half.w, ~=, cos_22_5);
    RC_CHECK_TRUE(rc_quatf_is_nearly_equal(squared, rc_quatf_mul(quarter, quarter), 1e-6f));

    // A real quaternion has a real logarithm, and the exponential of zero is the identity
    RC_CHECK(log_of_real.xyz, ==, rc_vec3f_make_zero());
    RC_CHECK(log_of_real.w, ~=, 0.69314718f);
    RC_CHECK_TRUE(rc_quatf_is_equal(rc_quatf_exp(rc_quatf_make(0.0f, 0.0f, 0.0f, 0.0f)), rc_quatf_make_identity()));
}
