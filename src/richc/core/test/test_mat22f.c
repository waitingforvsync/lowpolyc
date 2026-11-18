// test_mat22f.c
//
// Tests for rc_mat22f. The rotations are a quarter turn, whose entries are exact, and the 3-4-5
// turn with cos 0.6 and sin 0.8.

#include "richc/math/mat22f.h"

#include "richc/test.h"


// The anticlockwise quarter turn.
static rc_mat22f quarter_turn(void)
{
    return rc_mat22f_make(rc_vec2f_make(0.0f, 1.0f), rc_vec2f_make(-1.0f, 0.0f));
}


// The anticlockwise turn by the angle whose cosine is 0.6 and sine 0.8.
static rc_mat22f turn_3_4_5(void)
{
    return rc_mat22f_make(rc_vec2f_make(0.6f, 0.8f), rc_vec2f_make(-0.8f, 0.6f));
}


RC_TEST(mat22f, make)
{
    float elements[] = {1.0f, 2.0f, 3.0f, 4.0f};
    rc_mat22f m = rc_mat22f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.0f, 4.0f));
    rc_mat22f zero = rc_mat22f_make_zero();
    rc_mat22f identity = rc_mat22f_make_identity();
    rc_mat22f from_floats = rc_mat22f_from_floats(elements);

    RC_CHECK(m.cx, ==, rc_vec2f_make(1.0f, 2.0f));
    RC_CHECK(m.cy, ==, rc_vec2f_make(3.0f, 4.0f));
    RC_CHECK(zero.cx, ==, rc_vec2f_make_zero());
    RC_CHECK(zero.cy, ==, rc_vec2f_make_zero());
    RC_CHECK(identity.cx, ==, rc_vec2f_make(1.0f, 0.0f));
    RC_CHECK(identity.cy, ==, rc_vec2f_make(0.0f, 1.0f));

    // Column by column, both ways
    RC_CHECK(from_floats.cx, ==, rc_vec2f_make(1.0f, 2.0f));
    RC_CHECK(from_floats.cy, ==, rc_vec2f_make(3.0f, 4.0f));
    RC_CHECK_TRUE(rc_mat22f_as_floats(&m) == &m.cx.x);
    RC_CHECK(rc_mat22f_as_floats(&m)[2], ==, 3.0f);
}


RC_TEST(mat22f, arithmetic)
{
    rc_mat22f a = rc_mat22f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.0f, 4.0f));
    rc_mat22f b = rc_mat22f_make(rc_vec2f_make(10.0f, 20.0f), rc_vec2f_make(30.0f, 40.0f));
    rc_mat22f sum = rc_mat22f_add(a, b);
    rc_mat22f difference = rc_mat22f_sub(b, a);
    rc_mat22f scaled = rc_mat22f_scalar_mul(a, 2.0f);

    RC_CHECK(sum.cx, ==, rc_vec2f_make(11.0f, 22.0f));
    RC_CHECK(sum.cy, ==, rc_vec2f_make(33.0f, 44.0f));
    RC_CHECK(difference.cx, ==, rc_vec2f_make(9.0f, 18.0f));
    RC_CHECK(difference.cy, ==, rc_vec2f_make(27.0f, 36.0f));
    RC_CHECK(scaled.cx, ==, rc_vec2f_make(2.0f, 4.0f));
    RC_CHECK(scaled.cy, ==, rc_vec2f_make(6.0f, 8.0f));
}


RC_TEST(mat22f, vec2f_mul)
{
    rc_mat22f scale = rc_mat22f_make(rc_vec2f_make(2.0f, 0.0f), rc_vec2f_make(0.0f, 3.0f));
    rc_mat22f m = rc_mat22f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.0f, 4.0f));

    RC_CHECK(rc_mat22f_vec2f_mul(scale, rc_vec2f_make(1.0f, 1.0f)), ==, rc_vec2f_make(2.0f, 3.0f));
    RC_CHECK(rc_mat22f_vec2f_mul(scale, rc_vec2f_make(5.0f, 7.0f)), ==, rc_vec2f_make(10.0f, 21.0f));

    // The columns are where the axes go
    RC_CHECK(rc_mat22f_vec2f_mul(m, rc_vec2f_make_unitx()), ==, m.cx);
    RC_CHECK(rc_mat22f_vec2f_mul(m, rc_vec2f_make_unity()), ==, m.cy);
    RC_CHECK(rc_mat22f_vec2f_mul(m, rc_vec2f_make(1.0f, -1.0f)), ==, rc_vec2f_make(-2.0f, -2.0f));
}


RC_TEST(mat22f, rotations)
{
    rc_mat22f quarter = quarter_turn();
    rc_mat22f turn = turn_3_4_5();

    // A quarter turn sends +x to +y and +y to -x
    RC_CHECK(rc_mat22f_vec2f_mul(quarter, rc_vec2f_make_unitx()), ==, rc_vec2f_make_unity());
    RC_CHECK(rc_mat22f_vec2f_mul(quarter, rc_vec2f_make_unity()), ==, rc_vec2f_make(-1.0f, 0.0f));
    RC_CHECK(rc_mat22f_vec2f_mul(quarter, rc_vec2f_make(2.0f, 3.0f)), ==, rc_vec2f_perp(rc_vec2f_make(2.0f, 3.0f)));

    // A rotation keeps lengths
    RC_CHECK(rc_mat22f_vec2f_mul(turn, rc_vec2f_make(3.0f, 4.0f)), ~=, rc_vec2f_make(-1.4f, 4.8f));
    RC_CHECK(rc_vec2f_length(rc_mat22f_vec2f_mul(turn, rc_vec2f_make(3.0f, 4.0f))), ~=, 5.0f);
}


RC_TEST(mat22f, mul)
{
    rc_mat22f a = rc_mat22f_make(rc_vec2f_make(2.0f, 0.0f), rc_vec2f_make(0.0f, 3.0f));
    rc_mat22f b = rc_mat22f_make(rc_vec2f_make(1.0f, 1.0f), rc_vec2f_make(1.0f, -1.0f));
    rc_mat22f product = rc_mat22f_mul(a, b);
    rc_mat22f reversed = rc_mat22f_mul(b, a);
    rc_mat22f by_identity = rc_mat22f_mul(a, rc_mat22f_make_identity());
    rc_mat22f half_turn = rc_mat22f_mul(quarter_turn(), quarter_turn());

    // Each column of a * b is a times that column of b
    RC_CHECK(product.cx, ==, rc_vec2f_make(2.0f, 3.0f));
    RC_CHECK(product.cy, ==, rc_vec2f_make(2.0f, -3.0f));

    // b * a is a first, then b, which differs
    RC_CHECK(reversed.cx, ==, rc_vec2f_make(2.0f, 2.0f));
    RC_CHECK(reversed.cy, ==, rc_vec2f_make(3.0f, -3.0f));
    RC_CHECK(by_identity.cx, ==, a.cx);
    RC_CHECK(by_identity.cy, ==, a.cy);
    RC_CHECK(half_turn.cx, ==, rc_vec2f_make(-1.0f, 0.0f));
    RC_CHECK(half_turn.cy, ==, rc_vec2f_make(0.0f, -1.0f));
}


RC_TEST(mat22f, determinant)
{
    rc_mat22f m = rc_mat22f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.0f, 4.0f));
    rc_mat22f singular = rc_mat22f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(2.0f, 4.0f));

    RC_CHECK(rc_mat22f_determinant(m), ==, -2.0f);
    RC_CHECK(rc_mat22f_determinant(rc_mat22f_transpose(m)), ==, -2.0f);
    RC_CHECK(rc_mat22f_determinant(singular), ==, 0.0f);
    RC_CHECK(rc_mat22f_determinant(rc_mat22f_make_identity()), ==, 1.0f);
    RC_CHECK(rc_mat22f_determinant(quarter_turn()), ==, 1.0f);
    RC_CHECK(rc_mat22f_determinant(turn_3_4_5()), ~=, 1.0f);
}


RC_TEST(mat22f, transpose)
{
    rc_mat22f m = rc_mat22f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.0f, 4.0f));
    rc_mat22f transposed = rc_mat22f_transpose(m);
    rc_mat22f back = rc_mat22f_transpose(transposed);

    RC_CHECK(transposed.cx, ==, rc_vec2f_make(1.0f, 3.0f));
    RC_CHECK(transposed.cy, ==, rc_vec2f_make(2.0f, 4.0f));
    RC_CHECK(back.cx, ==, m.cx);
    RC_CHECK(back.cy, ==, m.cy);
}


RC_TEST(mat22f, inverse)
{
    rc_mat22f scale = rc_mat22f_make(rc_vec2f_make(2.0f, 0.0f), rc_vec2f_make(0.0f, 4.0f));
    rc_mat22f scale_inverse = rc_mat22f_inverse(scale);
    rc_mat22f m = rc_mat22f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.0f, 4.0f));
    rc_mat22f m_inverse = rc_mat22f_inverse(m);
    rc_mat22f product = rc_mat22f_mul(m, m_inverse);

    RC_CHECK(scale_inverse.cx, ==, rc_vec2f_make(0.5f, 0.0f));
    RC_CHECK(scale_inverse.cy, ==, rc_vec2f_make(0.0f, 0.25f));
    RC_CHECK(m_inverse.cx, ==, rc_vec2f_make(-2.0f, 1.0f));
    RC_CHECK(m_inverse.cy, ==, rc_vec2f_make(1.5f, -0.5f));
    RC_CHECK(product.cx, ==, rc_vec2f_make(1.0f, 0.0f));
    RC_CHECK(product.cy, ==, rc_vec2f_make(0.0f, 1.0f));
}


RC_TEST(mat22f, inverse_of_a_rotation_is_its_transpose)
{
    rc_mat22f quarter_inverse = rc_mat22f_inverse(quarter_turn());
    rc_mat22f quarter_transpose = rc_mat22f_transpose(quarter_turn());
    rc_mat22f turn_inverse = rc_mat22f_inverse(turn_3_4_5());
    rc_mat22f turn_transpose = rc_mat22f_transpose(turn_3_4_5());

    RC_CHECK(quarter_inverse.cx, ==, quarter_transpose.cx);
    RC_CHECK(quarter_inverse.cy, ==, quarter_transpose.cy);
    RC_CHECK(turn_inverse.cx, ~=, turn_transpose.cx);
    RC_CHECK(turn_inverse.cy, ~=, turn_transpose.cy);
}


RC_TEST(mat22f, make_rotation)
{
    rc_mat22f quarter = rc_mat22f_make_rotation(1.5707963f);
    rc_mat22f thirty = rc_mat22f_make_rotation(0.52359878f);
    rc_mat22f none = rc_mat22f_make_rotation(0.0f);

    // A quarter turn takes x to y, and y to -x
    RC_CHECK(rc_mat22f_vec2f_mul(quarter, rc_vec2f_make_unitx()), ~=, rc_vec2f_make_unity());
    RC_CHECK(rc_mat22f_vec2f_mul(quarter, rc_vec2f_make_unity()), ~=, rc_vec2f_make(-1.0f, 0.0f));

    // 30 degrees: the columns are (cos, sin) and (-sin, cos), and lengths are kept
    RC_CHECK(thirty.cx, ~=, rc_vec2f_make(0.8660254f, 0.5f));
    RC_CHECK(thirty.cy, ~=, rc_vec2f_make(-0.5f, 0.8660254f));
    RC_CHECK(rc_mat22f_determinant(thirty), ~=, 1.0f);
    RC_CHECK(rc_vec2f_length(rc_mat22f_vec2f_mul(thirty, rc_vec2f_make(3.0f, 4.0f))), ~=, 5.0f);

    // No turn is exactly the identity
    RC_CHECK(none.cx, ==, rc_vec2f_make_unitx());
    RC_CHECK(none.cy, ==, rc_vec2f_make_unity());
}
