// test_mat33f.c
//
// Tests for rc_mat33f. The rotations are a quarter turn about z, whose entries are exact, and the
// 3-4-5 turn about y with cos 0.6 and sin 0.8.

#include "richc/math/mat33f.h"

#include "richc/test.h"


// The right-handed quarter turn about z, taking +x to +y.
static rc_mat33f quarter_turn_z(void)
{
    return rc_mat33f_make_transpose(rc_vec3f_make(0.0f, -1.0f, 0.0f), rc_vec3f_make(1.0f, 0.0f, 0.0f), rc_vec3f_make_unitz());
}


// The right-handed turn about y by the angle whose cosine is 0.6 and sine 0.8.
static rc_mat33f turn_3_4_5_y(void)
{
    return rc_mat33f_make_transpose(rc_vec3f_make(0.6f, 0.0f, 0.8f), rc_vec3f_make_unity(), rc_vec3f_make(-0.8f, 0.0f, 0.6f));
}


// The matrix with columns (1, 2, 3), (0, 1, 4) and (5, 6, 0), whose determinant is 1.
static rc_mat33f general(void)
{
    return rc_mat33f_make(rc_vec3f_make(1.0f, 2.0f, 3.0f), rc_vec3f_make(0.0f, 1.0f, 4.0f), rc_vec3f_make(5.0f, 6.0f, 0.0f));
}


RC_TEST(mat33f, make)
{
    float elements[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
    rc_vec3f row_x = rc_vec3f_make(1.0f, 2.0f, 3.0f);
    rc_vec3f row_y = rc_vec3f_make(4.0f, 5.0f, 6.0f);
    rc_vec3f row_z = rc_vec3f_make(7.0f, 8.0f, 9.0f);
    rc_mat33f m = rc_mat33f_make(row_x, row_y, row_z);
    rc_mat33f from_rows = rc_mat33f_make_transpose(row_x, row_y, row_z);
    rc_mat33f zero = rc_mat33f_make_zero();
    rc_mat33f identity = rc_mat33f_make_identity();
    rc_mat33f from_floats = rc_mat33f_from_floats(elements);

    RC_CHECK(m.cx, ==, rc_vec3f_make(1.0f, 2.0f, 3.0f));
    RC_CHECK(m.cy, ==, rc_vec3f_make(4.0f, 5.0f, 6.0f));
    RC_CHECK(m.cz, ==, rc_vec3f_make(7.0f, 8.0f, 9.0f));
    RC_CHECK(from_rows.cx, ==, rc_vec3f_make(1.0f, 4.0f, 7.0f));
    RC_CHECK(from_rows.cy, ==, rc_vec3f_make(2.0f, 5.0f, 8.0f));
    RC_CHECK(from_rows.cz, ==, rc_vec3f_make(3.0f, 6.0f, 9.0f));
    RC_CHECK(zero.cx, ==, rc_vec3f_make_zero());
    RC_CHECK(zero.cy, ==, rc_vec3f_make_zero());
    RC_CHECK(zero.cz, ==, rc_vec3f_make_zero());
    RC_CHECK(identity.cx, ==, rc_vec3f_make(1.0f, 0.0f, 0.0f));
    RC_CHECK(identity.cy, ==, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(identity.cz, ==, rc_vec3f_make(0.0f, 0.0f, 1.0f));

    // Column by column, both ways
    RC_CHECK(from_floats.cx, ==, rc_vec3f_make(1.0f, 2.0f, 3.0f));
    RC_CHECK(from_floats.cy, ==, rc_vec3f_make(4.0f, 5.0f, 6.0f));
    RC_CHECK(from_floats.cz, ==, rc_vec3f_make(7.0f, 8.0f, 9.0f));
    RC_CHECK_TRUE(rc_mat33f_as_floats(&m) == &m.cx.x);
    RC_CHECK(rc_mat33f_as_floats(&m)[5], ==, 6.0f);
}


RC_TEST(mat33f, arithmetic)
{
    rc_mat33f a = rc_mat33f_make(rc_vec3f_make(1.0f, 2.0f, 3.0f), rc_vec3f_make(4.0f, 5.0f, 6.0f), rc_vec3f_make(7.0f, 8.0f, 9.0f));
    rc_mat33f b = rc_mat33f_scalar_mul(a, 10.0f);
    rc_mat33f sum = rc_mat33f_add(a, b);
    rc_mat33f difference = rc_mat33f_sub(b, a);

    RC_CHECK(b.cx, ==, rc_vec3f_make(10.0f, 20.0f, 30.0f));
    RC_CHECK(b.cy, ==, rc_vec3f_make(40.0f, 50.0f, 60.0f));
    RC_CHECK(b.cz, ==, rc_vec3f_make(70.0f, 80.0f, 90.0f));
    RC_CHECK(sum.cx, ==, rc_vec3f_make(11.0f, 22.0f, 33.0f));
    RC_CHECK(sum.cy, ==, rc_vec3f_make(44.0f, 55.0f, 66.0f));
    RC_CHECK(sum.cz, ==, rc_vec3f_make(77.0f, 88.0f, 99.0f));
    RC_CHECK(difference.cx, ==, rc_vec3f_make(9.0f, 18.0f, 27.0f));
    RC_CHECK(difference.cy, ==, rc_vec3f_make(36.0f, 45.0f, 54.0f));
    RC_CHECK(difference.cz, ==, rc_vec3f_make(63.0f, 72.0f, 81.0f));
}


RC_TEST(mat33f, vec3f_mul)
{
    rc_mat33f scale = rc_mat33f_make(rc_vec3f_make(2.0f, 0.0f, 0.0f), rc_vec3f_make(0.0f, 3.0f, 0.0f), rc_vec3f_make(0.0f, 0.0f, 4.0f));
    rc_mat33f m = general();

    RC_CHECK(rc_mat33f_vec3f_mul(scale, rc_vec3f_make(1.0f, 1.0f, 1.0f)), ==, rc_vec3f_make(2.0f, 3.0f, 4.0f));

    // The columns are where the axes go
    RC_CHECK(rc_mat33f_vec3f_mul(m, rc_vec3f_make_unitx()), ==, m.cx);
    RC_CHECK(rc_mat33f_vec3f_mul(m, rc_vec3f_make_unity()), ==, m.cy);
    RC_CHECK(rc_mat33f_vec3f_mul(m, rc_vec3f_make_unitz()), ==, m.cz);
    RC_CHECK(rc_mat33f_vec3f_mul(m, rc_vec3f_make(1.0f, 1.0f, -1.0f)), ==, rc_vec3f_make(-4.0f, -3.0f, 7.0f));
}


RC_TEST(mat33f, rotations)
{
    rc_mat33f quarter = quarter_turn_z();
    rc_mat33f turn = turn_3_4_5_y();

    // Right-handed: a quarter turn about z sends +x to +y and +y to -x, and leaves z be
    RC_CHECK(rc_mat33f_vec3f_mul(quarter, rc_vec3f_make_unitx()), ==, rc_vec3f_make_unity());
    RC_CHECK(rc_mat33f_vec3f_mul(quarter, rc_vec3f_make_unity()), ==, rc_vec3f_make(-1.0f, 0.0f, 0.0f));
    RC_CHECK(rc_mat33f_vec3f_mul(quarter, rc_vec3f_make_unitz()), ==, rc_vec3f_make_unitz());

    // About y, z turns towards x, and lengths are kept
    RC_CHECK(rc_mat33f_vec3f_mul(turn, rc_vec3f_make_unitz()), ~=, rc_vec3f_make(0.8f, 0.0f, 0.6f));
    RC_CHECK(rc_mat33f_vec3f_mul(turn, rc_vec3f_make_unity()), ==, rc_vec3f_make_unity());
    RC_CHECK(rc_vec3f_length(rc_mat33f_vec3f_mul(turn, rc_vec3f_make(3.0f, 4.0f, 12.0f))), ~=, 13.0f);
}


RC_TEST(mat33f, mul)
{
    rc_mat33f scale = rc_mat33f_make(rc_vec3f_make(2.0f, 0.0f, 0.0f), rc_vec3f_make(0.0f, 3.0f, 0.0f), rc_vec3f_make(0.0f, 0.0f, 4.0f));
    rc_mat33f shear = rc_mat33f_make(rc_vec3f_make(1.0f, 0.0f, 0.0f), rc_vec3f_make(1.0f, 1.0f, 0.0f), rc_vec3f_make(1.0f, 1.0f, 1.0f));
    rc_mat33f product = rc_mat33f_mul(scale, shear);
    rc_mat33f reversed = rc_mat33f_mul(shear, scale);
    rc_mat33f by_identity = rc_mat33f_mul(general(), rc_mat33f_make_identity());
    rc_mat33f half_turn = rc_mat33f_mul(quarter_turn_z(), quarter_turn_z());

    // Each column of a * b is a times that column of b
    RC_CHECK(product.cx, ==, rc_vec3f_make(2.0f, 0.0f, 0.0f));
    RC_CHECK(product.cy, ==, rc_vec3f_make(2.0f, 3.0f, 0.0f));
    RC_CHECK(product.cz, ==, rc_vec3f_make(2.0f, 3.0f, 4.0f));
    RC_CHECK(reversed.cx, ==, rc_vec3f_make(2.0f, 0.0f, 0.0f));
    RC_CHECK(reversed.cy, ==, rc_vec3f_make(3.0f, 3.0f, 0.0f));
    RC_CHECK(reversed.cz, ==, rc_vec3f_make(4.0f, 4.0f, 4.0f));
    RC_CHECK(by_identity.cx, ==, general().cx);
    RC_CHECK(by_identity.cy, ==, general().cy);
    RC_CHECK(by_identity.cz, ==, general().cz);
    RC_CHECK(half_turn.cx, ==, rc_vec3f_make(-1.0f, 0.0f, 0.0f));
    RC_CHECK(half_turn.cy, ==, rc_vec3f_make(0.0f, -1.0f, 0.0f));
    RC_CHECK(half_turn.cz, ==, rc_vec3f_make_unitz());
}


RC_TEST(mat33f, determinant)
{
    rc_mat33f scale = rc_mat33f_make(rc_vec3f_make(2.0f, 0.0f, 0.0f), rc_vec3f_make(0.0f, 3.0f, 0.0f), rc_vec3f_make(0.0f, 0.0f, 4.0f));
    rc_mat33f singular = rc_mat33f_make(rc_vec3f_make(1.0f, 2.0f, 3.0f), rc_vec3f_make(4.0f, 5.0f, 6.0f), rc_vec3f_make(7.0f, 8.0f, 9.0f));
    rc_mat33f mirror = rc_mat33f_make(rc_vec3f_make(-1.0f, 0.0f, 0.0f), rc_vec3f_make_unity(), rc_vec3f_make_unitz());

    RC_CHECK(rc_mat33f_determinant(rc_mat33f_make_identity()), ==, 1.0f);
    RC_CHECK(rc_mat33f_determinant(scale), ==, 24.0f);
    RC_CHECK(rc_mat33f_determinant(general()), ==, 1.0f);
    RC_CHECK(rc_mat33f_determinant(rc_mat33f_transpose(general())), ==, 1.0f);
    RC_CHECK(rc_mat33f_determinant(singular), ==, 0.0f);
    RC_CHECK(rc_mat33f_determinant(mirror), ==, -1.0f);
    RC_CHECK(rc_mat33f_determinant(quarter_turn_z()), ==, 1.0f);
    RC_CHECK(rc_mat33f_determinant(turn_3_4_5_y()), ~=, 1.0f);
}


RC_TEST(mat33f, transpose)
{
    rc_mat33f m = general();
    rc_mat33f transposed = rc_mat33f_transpose(m);
    rc_mat33f back = rc_mat33f_transpose(transposed);

    RC_CHECK(transposed.cx, ==, rc_vec3f_make(1.0f, 0.0f, 5.0f));
    RC_CHECK(transposed.cy, ==, rc_vec3f_make(2.0f, 1.0f, 6.0f));
    RC_CHECK(transposed.cz, ==, rc_vec3f_make(3.0f, 4.0f, 0.0f));
    RC_CHECK(back.cx, ==, m.cx);
    RC_CHECK(back.cy, ==, m.cy);
    RC_CHECK(back.cz, ==, m.cz);
}


RC_TEST(mat33f, inverse)
{
    rc_mat33f scale = rc_mat33f_make(rc_vec3f_make(2.0f, 0.0f, 0.0f), rc_vec3f_make(0.0f, 4.0f, 0.0f), rc_vec3f_make(0.0f, 0.0f, 5.0f));
    rc_mat33f scale_inverse = rc_mat33f_inverse(scale);
    rc_mat33f m_inverse = rc_mat33f_inverse(general());
    rc_mat33f product = rc_mat33f_mul(general(), m_inverse);

    RC_CHECK(scale_inverse.cx, ~=, rc_vec3f_make(0.5f, 0.0f, 0.0f));
    RC_CHECK(scale_inverse.cy, ~=, rc_vec3f_make(0.0f, 0.25f, 0.0f));
    RC_CHECK(scale_inverse.cz, ~=, rc_vec3f_make(0.0f, 0.0f, 0.2f));

    // With a determinant of 1 the inverse is the adjugate, all whole numbers
    RC_CHECK(m_inverse.cx, ==, rc_vec3f_make(-24.0f, 18.0f, 5.0f));
    RC_CHECK(m_inverse.cy, ==, rc_vec3f_make(20.0f, -15.0f, -4.0f));
    RC_CHECK(m_inverse.cz, ==, rc_vec3f_make(-5.0f, 4.0f, 1.0f));
    RC_CHECK(product.cx, ==, rc_vec3f_make(1.0f, 0.0f, 0.0f));
    RC_CHECK(product.cy, ==, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(product.cz, ==, rc_vec3f_make(0.0f, 0.0f, 1.0f));
}


RC_TEST(mat33f, inverse_of_a_rotation_is_its_transpose)
{
    rc_mat33f quarter_inverse = rc_mat33f_inverse(quarter_turn_z());
    rc_mat33f quarter_transpose = rc_mat33f_transpose(quarter_turn_z());
    rc_mat33f turn_inverse = rc_mat33f_inverse(turn_3_4_5_y());
    rc_mat33f turn_transpose = rc_mat33f_transpose(turn_3_4_5_y());

    RC_CHECK(quarter_inverse.cx, ==, quarter_transpose.cx);
    RC_CHECK(quarter_inverse.cy, ==, quarter_transpose.cy);
    RC_CHECK(quarter_inverse.cz, ==, quarter_transpose.cz);
    RC_CHECK(turn_inverse.cx, ~=, turn_transpose.cx);
    RC_CHECK(turn_inverse.cy, ~=, turn_transpose.cy);
    RC_CHECK(turn_inverse.cz, ~=, turn_transpose.cz);
}


RC_TEST(mat33f, make_rotation)
{
    rc_mat33f about_x = rc_mat33f_make_rotation_x(1.5707963f);
    rc_mat33f about_y = rc_mat33f_make_rotation_y(1.5707963f);
    rc_mat33f about_z = rc_mat33f_make_rotation_z(1.5707963f);
    rc_mat33f thirty_z = rc_mat33f_make_rotation_z(0.52359878f);
    rc_mat33f none = rc_mat33f_make_rotation_y(0.0f);

    // Right-handed quarter turns: y to z about x, z to x about y, and x to y about z
    RC_CHECK(rc_mat33f_vec3f_mul(about_x, rc_vec3f_make_unity()), ~=, rc_vec3f_make_unitz());
    RC_CHECK(rc_mat33f_vec3f_mul(about_y, rc_vec3f_make_unitz()), ~=, rc_vec3f_make_unitx());
    RC_CHECK(rc_mat33f_vec3f_mul(about_z, rc_vec3f_make_unitx()), ~=, rc_vec3f_make_unity());

    // Each leaves its own axis where it is
    RC_CHECK(rc_mat33f_vec3f_mul(about_x, rc_vec3f_make_unitx()), ==, rc_vec3f_make_unitx());
    RC_CHECK(rc_mat33f_vec3f_mul(about_y, rc_vec3f_make_unity()), ==, rc_vec3f_make_unity());
    RC_CHECK(rc_mat33f_vec3f_mul(about_z, rc_vec3f_make_unitz()), ==, rc_vec3f_make_unitz());

    // 30 degrees about z
    RC_CHECK(thirty_z.cx, ~=, rc_vec3f_make(0.8660254f, 0.5f, 0.0f));
    RC_CHECK(thirty_z.cy, ~=, rc_vec3f_make(-0.5f, 0.8660254f, 0.0f));
    RC_CHECK(thirty_z.cz, ==, rc_vec3f_make_unitz());
    RC_CHECK(rc_mat33f_determinant(thirty_z), ~=, 1.0f);

    // No turn is exactly the identity
    RC_CHECK(none.cx, ==, rc_vec3f_make_unitx());
    RC_CHECK(none.cy, ==, rc_vec3f_make_unity());
    RC_CHECK(none.cz, ==, rc_vec3f_make_unitz());
}
