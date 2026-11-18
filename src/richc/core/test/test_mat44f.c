// test_mat44f.c
//
// Tests for rc_mat44f, including the orthographic projections into our reversed-depth clip space.

#include "richc/math/mat44f.h"

#include "richc/test.h"


// The right-handed quarter turn about z, taking +x to +y, whose entries are exact.
static rc_mat33f quarter_turn_z(void)
{
    return rc_mat33f_make_transpose(rc_vec3f_make(0.0f, -1.0f, 0.0f), rc_vec3f_make(1.0f, 0.0f, 0.0f), rc_vec3f_make_unitz());
}


// An affine matrix of small whole numbers, whose determinant is 25.
static rc_mat44f general(void)
{
    return rc_mat44f_make(
        rc_vec4f_make(2.0f, 1.0f, 0.0f, 0.0f),
        rc_vec4f_make(0.0f, 3.0f, 1.0f, 0.0f),
        rc_vec4f_make(1.0f, 0.0f, 4.0f, 0.0f),
        rc_vec4f_make(5.0f, 6.0f, 7.0f, 1.0f)
    );
}


RC_TEST(mat44f, make)
{
    float elements[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};
    rc_vec4f row_x = rc_vec4f_make(1.0f, 2.0f, 3.0f, 4.0f);
    rc_vec4f row_y = rc_vec4f_make(5.0f, 6.0f, 7.0f, 8.0f);
    rc_vec4f row_z = rc_vec4f_make(9.0f, 10.0f, 11.0f, 12.0f);
    rc_vec4f row_w = rc_vec4f_make(13.0f, 14.0f, 15.0f, 16.0f);
    rc_mat44f m = rc_mat44f_make(row_x, row_y, row_z, row_w);
    rc_mat44f from_rows = rc_mat44f_make_transpose(row_x, row_y, row_z, row_w);
    rc_mat44f zero = rc_mat44f_make_zero();
    rc_mat44f identity = rc_mat44f_make_identity();
    rc_mat44f from_floats = rc_mat44f_from_floats(elements);

    RC_CHECK(m.cx, ==, row_x);
    RC_CHECK(m.cw, ==, row_w);
    RC_CHECK(from_rows.cx, ==, rc_vec4f_make(1.0f, 5.0f, 9.0f, 13.0f));
    RC_CHECK(from_rows.cy, ==, rc_vec4f_make(2.0f, 6.0f, 10.0f, 14.0f));
    RC_CHECK(from_rows.cz, ==, rc_vec4f_make(3.0f, 7.0f, 11.0f, 15.0f));
    RC_CHECK(from_rows.cw, ==, rc_vec4f_make(4.0f, 8.0f, 12.0f, 16.0f));
    RC_CHECK(zero.cx, ==, rc_vec4f_make_zero());
    RC_CHECK(zero.cw, ==, rc_vec4f_make_zero());
    RC_CHECK(identity.cx, ==, rc_vec4f_make(1.0f, 0.0f, 0.0f, 0.0f));
    RC_CHECK(identity.cy, ==, rc_vec4f_make(0.0f, 1.0f, 0.0f, 0.0f));
    RC_CHECK(identity.cz, ==, rc_vec4f_make(0.0f, 0.0f, 1.0f, 0.0f));
    RC_CHECK(identity.cw, ==, rc_vec4f_make(0.0f, 0.0f, 0.0f, 1.0f));

    // Column by column, both ways
    RC_CHECK(from_floats.cx, ==, rc_vec4f_make(1.0f, 2.0f, 3.0f, 4.0f));
    RC_CHECK(from_floats.cz, ==, rc_vec4f_make(9.0f, 10.0f, 11.0f, 12.0f));
    RC_CHECK(from_floats.cw, ==, rc_vec4f_make(13.0f, 14.0f, 15.0f, 16.0f));
    RC_CHECK_TRUE(rc_mat44f_as_floats(&m) == &m.cx.x);
    RC_CHECK(rc_mat44f_as_floats(&m)[14], ==, 15.0f);
}


RC_TEST(mat44f, from_smaller_matrices)
{
    rc_mat22f m22 = rc_mat22f_make(rc_vec2f_make(2.0f, 1.0f), rc_vec2f_make(-1.0f, 3.0f));
    rc_mat33f m33 = rc_mat33f_make(rc_vec3f_make(2.0f, 0.0f, 1.0f), rc_vec3f_make(0.0f, 3.0f, 0.0f), rc_vec3f_make(-1.0f, 0.0f, 4.0f));
    rc_mat34f m34 = rc_mat34f_make(m33, rc_vec3f_make(5.0f, 6.0f, 7.0f));
    rc_vec3f point = rc_vec3f_make(1.0f, -2.0f, 3.0f);
    rc_mat44f from22 = rc_mat44f_from_mat22f(m22);
    rc_mat44f from33 = rc_mat44f_from_mat33f(m33);
    rc_mat44f from34 = rc_mat44f_from_mat34f(m34);

    RC_CHECK(from22.cx, ==, rc_vec4f_make(2.0f, 1.0f, 0.0f, 0.0f));
    RC_CHECK(from22.cy, ==, rc_vec4f_make(-1.0f, 3.0f, 0.0f, 0.0f));
    RC_CHECK(from22.cz, ==, rc_vec4f_make(0.0f, 0.0f, 1.0f, 0.0f));
    RC_CHECK(from22.cw, ==, rc_vec4f_make(0.0f, 0.0f, 0.0f, 1.0f));
    RC_CHECK(from33.cx, ==, rc_vec4f_make(2.0f, 0.0f, 1.0f, 0.0f));
    RC_CHECK(from33.cy, ==, rc_vec4f_make(0.0f, 3.0f, 0.0f, 0.0f));
    RC_CHECK(from33.cz, ==, rc_vec4f_make(-1.0f, 0.0f, 4.0f, 0.0f));
    RC_CHECK(from33.cw, ==, rc_vec4f_make(0.0f, 0.0f, 0.0f, 1.0f));

    // The bottom row of a transform is (0, 0, 0, 1)
    RC_CHECK(from34.cx, ==, rc_vec4f_make(2.0f, 0.0f, 1.0f, 0.0f));
    RC_CHECK(from34.cz, ==, rc_vec4f_make(-1.0f, 0.0f, 4.0f, 0.0f));
    RC_CHECK(from34.cw, ==, rc_vec4f_make(5.0f, 6.0f, 7.0f, 1.0f));
    RC_CHECK(rc_mat44f_vec4f_mul(from34, rc_vec4f_from_vec3f(point, 1.0f)), ==, rc_vec4f_from_vec3f(rc_mat34f_vec3f_mul(m34, point), 1.0f));
}


RC_TEST(mat44f, arithmetic)
{
    rc_mat44f a = general();
    rc_mat44f b = rc_mat44f_make_identity();
    rc_mat44f sum = rc_mat44f_add(a, b);
    rc_mat44f difference = rc_mat44f_sub(a, b);
    rc_mat44f scaled = rc_mat44f_scalar_mul(a, -2.0f);

    RC_CHECK(sum.cx, ==, rc_vec4f_make(3.0f, 1.0f, 0.0f, 0.0f));
    RC_CHECK(sum.cw, ==, rc_vec4f_make(5.0f, 6.0f, 7.0f, 2.0f));
    RC_CHECK(difference.cy, ==, rc_vec4f_make(0.0f, 2.0f, 1.0f, 0.0f));
    RC_CHECK(difference.cw, ==, rc_vec4f_make(5.0f, 6.0f, 7.0f, 0.0f));
    RC_CHECK(scaled.cz, ==, rc_vec4f_make(-2.0f, 0.0f, -8.0f, 0.0f));
    RC_CHECK(scaled.cw, ==, rc_vec4f_make(-10.0f, -12.0f, -14.0f, -2.0f));
}


RC_TEST(mat44f, vec4f_mul_and_mul)
{
    rc_mat44f scale = rc_mat44f_make(
        rc_vec4f_make(2.0f, 0.0f, 0.0f, 0.0f),
        rc_vec4f_make(0.0f, 3.0f, 0.0f, 0.0f),
        rc_vec4f_make(0.0f, 0.0f, 4.0f, 0.0f),
        rc_vec4f_make(0.0f, 0.0f, 0.0f, 5.0f)
    );
    rc_mat44f by_identity = rc_mat44f_mul(general(), rc_mat44f_make_identity());
    rc_mat44f product = rc_mat44f_mul(scale, general());
    rc_mat44f reversed = rc_mat44f_mul(general(), scale);

    RC_CHECK(rc_mat44f_vec4f_mul(scale, rc_vec4f_make(1.0f, 1.0f, 1.0f, 1.0f)), ==, rc_vec4f_make(2.0f, 3.0f, 4.0f, 5.0f));
    RC_CHECK(rc_mat44f_vec4f_mul(general(), rc_vec4f_make_unitw()), ==, general().cw);
    RC_CHECK(by_identity.cx, ==, general().cx);
    RC_CHECK(by_identity.cw, ==, general().cw);

    // Scaling after scales the rows, and before scales the columns
    RC_CHECK(product.cx, ==, rc_vec4f_make(4.0f, 3.0f, 0.0f, 0.0f));
    RC_CHECK(product.cw, ==, rc_vec4f_make(10.0f, 18.0f, 28.0f, 5.0f));
    RC_CHECK(reversed.cx, ==, rc_vec4f_make(4.0f, 2.0f, 0.0f, 0.0f));
    RC_CHECK(reversed.cw, ==, rc_vec4f_make(25.0f, 30.0f, 35.0f, 5.0f));
}


RC_TEST(mat44f, translation)
{
    rc_mat44f translation = rc_mat44f_make_translation(rc_vec3f_make(5.0f, 6.0f, 7.0f));
    rc_mat44f there_and_back = rc_mat44f_mul(translation, rc_mat44f_make_translation(rc_vec3f_make(-5.0f, -6.0f, -7.0f)));

    // A point (w = 1) moves, and a direction (w = 0) doesn't
    RC_CHECK(rc_mat44f_vec4f_mul(translation, rc_vec4f_make(1.0f, 2.0f, 3.0f, 1.0f)), ==, rc_vec4f_make(6.0f, 8.0f, 10.0f, 1.0f));
    RC_CHECK(rc_mat44f_vec4f_mul(translation, rc_vec4f_make(1.0f, 2.0f, 3.0f, 0.0f)), ==, rc_vec4f_make(1.0f, 2.0f, 3.0f, 0.0f));
    RC_CHECK(there_and_back.cw, ==, rc_vec4f_make_unitw());
}


RC_TEST(mat44f, transpose)
{
    rc_mat44f transposed = rc_mat44f_transpose(general());
    rc_mat44f back = rc_mat44f_transpose(transposed);

    RC_CHECK(transposed.cx, ==, rc_vec4f_make(2.0f, 0.0f, 1.0f, 5.0f));
    RC_CHECK(transposed.cy, ==, rc_vec4f_make(1.0f, 3.0f, 0.0f, 6.0f));
    RC_CHECK(transposed.cz, ==, rc_vec4f_make(0.0f, 1.0f, 4.0f, 7.0f));
    RC_CHECK(transposed.cw, ==, rc_vec4f_make(0.0f, 0.0f, 0.0f, 1.0f));
    RC_CHECK(back.cx, ==, general().cx);
    RC_CHECK(back.cw, ==, general().cw);
}


RC_TEST(mat44f, ortho)
{
    rc_mat44f projection = rc_mat44f_make_ortho(0.0f, 4.0f, 2.0f, 0.0f, 1.0f, 5.0f);
    rc_vec4f point = rc_vec4f_make(1.5f, 0.5f, -2.5f, 1.0f);
    rc_vec4f back = rc_mat44f_vec4f_mul(rc_mat44f_inverse(projection), rc_mat44f_vec4f_mul(projection, point));

    // The near face lands on depth 1 and the far face on 0, while x and y run from -1 to 1
    RC_CHECK(rc_mat44f_vec4f_mul(projection, rc_vec4f_make(0.0f, 0.0f, -1.0f, 1.0f)), ==, rc_vec4f_make(-1.0f, -1.0f, 1.0f, 1.0f));
    RC_CHECK(rc_mat44f_vec4f_mul(projection, rc_vec4f_make(4.0f, 2.0f, -5.0f, 1.0f)), ==, rc_vec4f_make(1.0f, 1.0f, 0.0f, 1.0f));
    RC_CHECK(rc_mat44f_vec4f_mul(projection, rc_vec4f_make(0.0f, 2.0f, -5.0f, 1.0f)), ==, rc_vec4f_make(-1.0f, 1.0f, 0.0f, 1.0f));
    RC_CHECK(rc_mat44f_vec4f_mul(projection, rc_vec4f_make(4.0f, 0.0f, -1.0f, 1.0f)), ==, rc_vec4f_make(1.0f, -1.0f, 1.0f, 1.0f));
    RC_CHECK(rc_mat44f_vec4f_mul(projection, rc_vec4f_make(2.0f, 1.0f, -3.0f, 1.0f)), ==, rc_vec4f_make(0.0f, 0.0f, 0.5f, 1.0f));
    RC_CHECK(back, ~=, point);
}


RC_TEST(mat44f, ortho_2d)
{
    rc_mat44f projection = rc_mat44f_make_ortho_2d(640.0f, 480.0f);

    // y runs down the screen and up clip space; z = 0 is on the near plane
    RC_CHECK(rc_mat44f_vec4f_mul(projection, rc_vec4f_make(0.0f, 0.0f, 0.0f, 1.0f)), ~=, rc_vec4f_make(-1.0f, 1.0f, 1.0f, 1.0f));
    RC_CHECK(rc_mat44f_vec4f_mul(projection, rc_vec4f_make(640.0f, 480.0f, 0.0f, 1.0f)), ~=, rc_vec4f_make(1.0f, -1.0f, 1.0f, 1.0f));
    RC_CHECK(rc_mat44f_vec4f_mul(projection, rc_vec4f_make(320.0f, 240.0f, 0.0f, 1.0f)), ~=, rc_vec4f_make(0.0f, 0.0f, 1.0f, 1.0f));
    RC_CHECK(rc_mat44f_vec4f_mul(projection, rc_vec4f_make(640.0f, 0.0f, -1.0f, 1.0f)), ~=, rc_vec4f_make(1.0f, 1.0f, 0.0f, 1.0f));
}


RC_TEST(mat44f, determinant)
{
    rc_mat44f scale_and_move = rc_mat44f_make(
        rc_vec4f_make(1.0f, 0.0f, 0.0f, 0.0f),
        rc_vec4f_make(0.0f, 2.0f, 0.0f, 0.0f),
        rc_vec4f_make(0.0f, 0.0f, 3.0f, 0.0f),
        rc_vec4f_make(5.0f, 6.0f, 7.0f, 1.0f)
    );
    rc_mat44f singular = rc_mat44f_make(
        rc_vec4f_make(1.0f, 2.0f, 3.0f, 4.0f),
        rc_vec4f_make(2.0f, 4.0f, 6.0f, 8.0f),
        rc_vec4f_make(0.0f, 1.0f, 0.0f, 0.0f),
        rc_vec4f_make(0.0f, 0.0f, 1.0f, 0.0f)
    );
    rc_mat44f swap_xy = rc_mat44f_make(rc_vec4f_make_unity(), rc_vec4f_make_unitx(), rc_vec4f_make_unitz(), rc_vec4f_make_unitw());

    RC_CHECK(rc_mat44f_determinant(rc_mat44f_make_identity()), ==, 1.0f);
    RC_CHECK(rc_mat44f_determinant(rc_mat44f_make_translation(rc_vec3f_make(3.0f, 4.0f, 5.0f))), ==, 1.0f);
    RC_CHECK(rc_mat44f_determinant(scale_and_move), ==, 6.0f);
    RC_CHECK(rc_mat44f_determinant(general()), ==, 25.0f);
    RC_CHECK(rc_mat44f_determinant(rc_mat44f_transpose(general())), ==, 25.0f);
    RC_CHECK(rc_mat44f_determinant(singular), ==, 0.0f);
    RC_CHECK(rc_mat44f_determinant(swap_xy), ==, -1.0f);
}


RC_TEST(mat44f, inverse)
{
    rc_mat44f m_inverse = rc_mat44f_inverse(general());
    rc_mat44f product = rc_mat44f_mul(general(), m_inverse);
    rc_mat44f reversed = rc_mat44f_mul(m_inverse, general());

    // The cofactors over the determinant of 25
    RC_CHECK(m_inverse.cx, ~=, rc_vec4f_make(0.48f, -0.16f, 0.04f, 0.0f));
    RC_CHECK(m_inverse.cw, ~=, rc_vec4f_make(-1.8f, -1.4f, -1.4f, 1.0f));
    RC_CHECK(product.cx, ~=, rc_vec4f_make_unitx());
    RC_CHECK(product.cy, ~=, rc_vec4f_make_unity());
    RC_CHECK(product.cz, ~=, rc_vec4f_make_unitz());
    RC_CHECK(product.cw, ~=, rc_vec4f_make_unitw());
    RC_CHECK(reversed.cx, ~=, rc_vec4f_make_unitx());
    RC_CHECK(reversed.cw, ~=, rc_vec4f_make_unitw());
}


RC_TEST(mat44f, inverse_of_a_transform)
{
    rc_mat44f translation = rc_mat44f_make_translation(rc_vec3f_make(5.0f, -6.0f, 7.0f));
    rc_mat44f translation_inverse = rc_mat44f_inverse(translation);
    rc_mat34f transform = rc_mat34f_make(quarter_turn_z(), rc_vec3f_make(1.0f, 2.0f, 3.0f));
    rc_mat44f transform_inverse = rc_mat44f_inverse(rc_mat44f_from_mat34f(transform));
    rc_mat44f expected = rc_mat44f_from_mat34f(rc_mat34f_inverse(transform));

    RC_CHECK(translation_inverse.cw, ==, rc_vec4f_make(-5.0f, 6.0f, -7.0f, 1.0f));

    // Agrees with the affine inverse
    RC_CHECK(transform_inverse.cx, ==, expected.cx);
    RC_CHECK(transform_inverse.cy, ==, expected.cy);
    RC_CHECK(transform_inverse.cz, ==, expected.cz);
    RC_CHECK(transform_inverse.cw, ==, expected.cw);
}


RC_TEST(mat44f, perspective)
{
    rc_mat44f projection = rc_mat44f_make_perspective(1.5707963f, 2.0f, 1.0f, 5.0f);
    rc_vec4f near_centre = rc_mat44f_vec4f_mul(projection, rc_vec4f_make(0.0f, 0.0f, -1.0f, 1.0f));
    rc_vec4f far_centre = rc_mat44f_vec4f_mul(projection, rc_vec4f_make(0.0f, 0.0f, -5.0f, 1.0f));
    rc_vec4f edge = rc_mat44f_vec4f_mul(projection, rc_vec4f_make(2.0f, 1.0f, -1.0f, 1.0f));
    rc_vec4f middle = rc_mat44f_vec4f_mul(projection, rc_vec4f_make(0.3f, 0.7f, -3.0f, 1.0f));

    // A 90 degree field of view, 2:1, near 1 and far 5: the near plane lands on depth 1 and the
    // far one on depth 0
    RC_CHECK(near_centre, ~=, rc_vec4f_make(0.0f, 0.0f, 1.0f, 1.0f));
    RC_CHECK(near_centre.z / near_centre.w, ~=, 1.0f);
    RC_CHECK(far_centre, ~=, rc_vec4f_make(0.0f, 0.0f, 0.0f, 5.0f));

    // x is scaled by focal / aspect and y by focal, which is 1 here
    RC_CHECK(edge.x, ~=, 1.0f);
    RC_CHECK(edge.y, ~=, 1.0f);

    // Clip w is the distance in front of the camera
    RC_CHECK(middle.w, ~=, 3.0f);
}


RC_TEST(mat44f, perspective_inf)
{
    rc_mat44f infinite = rc_mat44f_make_perspective_inf(1.5707963f, 1.0f, 2.0f);
    rc_mat44f distant = rc_mat44f_make_perspective(1.5707963f, 1.0f, 2.0f, 100000.0f);
    rc_vec4f near_centre = rc_mat44f_vec4f_mul(infinite, rc_vec4f_make(0.0f, 0.0f, -2.0f, 1.0f));
    rc_vec4f far_point = rc_mat44f_vec4f_mul(infinite, rc_vec4f_make(0.0f, 0.0f, -1000.0f, 1.0f));
    rc_vec4f from_infinite = rc_mat44f_vec4f_mul(infinite, rc_vec4f_make(1.0f, -1.0f, -50.0f, 1.0f));
    rc_vec4f from_distant = rc_mat44f_vec4f_mul(distant, rc_vec4f_make(1.0f, -1.0f, -50.0f, 1.0f));

    // The near plane lands on depth 1, and a point further off on z_near / distance
    RC_CHECK(near_centre.z / near_centre.w, ~=, 1.0f);
    RC_CHECK(far_point.z / far_point.w, ~=, 2.0f / 1000.0f);

    // It agrees with a projection whose far plane is a long way off
    RC_CHECK(from_infinite.x, ~=, from_distant.x);
    RC_CHECK(from_infinite.y, ~=, from_distant.y);
    RC_CHECK(from_infinite.z / from_infinite.w, ~=, from_distant.z / from_distant.w);
    RC_CHECK(from_infinite.w, ~=, from_distant.w);
}
