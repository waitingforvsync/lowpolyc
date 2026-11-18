// test_mat34f.c
//
// Tests for rc_mat34f, the 3D affine transform.

#include "richc/math/mat34f.h"

#include "richc/test.h"


// The right-handed quarter turn about z, taking +x to +y, whose entries are exact.
static rc_mat33f quarter_turn_z(void)
{
    return rc_mat33f_make_transpose(rc_vec3f_make(0.0f, -1.0f, 0.0f), rc_vec3f_make(1.0f, 0.0f, 0.0f), rc_vec3f_make_unitz());
}


RC_TEST(mat34f, make)
{
    float elements[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f};
    rc_mat33f rot = rc_mat33f_make(rc_vec3f_make(1.0f, 2.0f, 3.0f), rc_vec3f_make(4.0f, 5.0f, 6.0f), rc_vec3f_make(7.0f, 8.0f, 9.0f));
    rc_mat34f m = rc_mat34f_make(rot, rc_vec3f_make(10.0f, 11.0f, 12.0f));
    rc_mat34f identity = rc_mat34f_make_identity();
    rc_mat34f translation = rc_mat34f_make_translation(rc_vec3f_make(5.0f, 6.0f, 7.0f));
    rc_mat34f linear = rc_mat34f_from_mat33f(rot);
    rc_mat34f from_floats = rc_mat34f_from_floats(elements);

    RC_CHECK(m.rot.cx, ==, rot.cx);
    RC_CHECK(m.rot.cy, ==, rot.cy);
    RC_CHECK(m.rot.cz, ==, rot.cz);
    RC_CHECK(m.trans, ==, rc_vec3f_make(10.0f, 11.0f, 12.0f));
    RC_CHECK(identity.rot.cx, ==, rc_vec3f_make(1.0f, 0.0f, 0.0f));
    RC_CHECK(identity.rot.cy, ==, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(identity.rot.cz, ==, rc_vec3f_make(0.0f, 0.0f, 1.0f));
    RC_CHECK(identity.trans, ==, rc_vec3f_make_zero());
    RC_CHECK(translation.rot.cx, ==, rc_vec3f_make(1.0f, 0.0f, 0.0f));
    RC_CHECK(translation.rot.cy, ==, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(translation.rot.cz, ==, rc_vec3f_make(0.0f, 0.0f, 1.0f));
    RC_CHECK(translation.trans, ==, rc_vec3f_make(5.0f, 6.0f, 7.0f));
    RC_CHECK(linear.rot.cx, ==, rot.cx);
    RC_CHECK(linear.rot.cz, ==, rot.cz);
    RC_CHECK(linear.trans, ==, rc_vec3f_make_zero());

    // Column by column, the translation last, both ways
    RC_CHECK(from_floats.rot.cx, ==, rc_vec3f_make(1.0f, 2.0f, 3.0f));
    RC_CHECK(from_floats.rot.cy, ==, rc_vec3f_make(4.0f, 5.0f, 6.0f));
    RC_CHECK(from_floats.rot.cz, ==, rc_vec3f_make(7.0f, 8.0f, 9.0f));
    RC_CHECK(from_floats.trans, ==, rc_vec3f_make(10.0f, 11.0f, 12.0f));
    RC_CHECK_TRUE(rc_mat34f_as_floats(&m) == &m.rot.cx.x);
    RC_CHECK(rc_mat34f_as_floats(&m)[9], ==, 10.0f);
}


RC_TEST(mat34f, vec3f_mul)
{
    rc_mat34f translation = rc_mat34f_make_translation(rc_vec3f_make(5.0f, 6.0f, 7.0f));
    rc_mat34f turn_then_move = rc_mat34f_make(quarter_turn_z(), rc_vec3f_make(0.0f, 0.0f, 5.0f));

    RC_CHECK(rc_mat34f_vec3f_mul(translation, rc_vec3f_make(1.0f, 2.0f, 3.0f)), ==, rc_vec3f_make(6.0f, 8.0f, 10.0f));
    RC_CHECK(rc_mat34f_vec3f_mul(rc_mat34f_make_identity(), rc_vec3f_make(1.0f, 2.0f, 3.0f)), ==, rc_vec3f_make(1.0f, 2.0f, 3.0f));

    // The quarter turn takes (1, 0, 0) to (0, 1, 0), and the move to (0, 1, 5)
    RC_CHECK(rc_mat34f_vec3f_mul(turn_then_move, rc_vec3f_make(1.0f, 0.0f, 0.0f)), ==, rc_vec3f_make(0.0f, 1.0f, 5.0f));
}


RC_TEST(mat34f, mul)
{
    rc_mat34f a = rc_mat34f_make_translation(rc_vec3f_make(10.0f, 0.0f, 0.0f));
    rc_mat34f b = rc_mat34f_make(quarter_turn_z(), rc_vec3f_make(0.0f, 0.0f, 2.0f));
    rc_mat34f ab = rc_mat34f_mul(a, b);
    rc_mat34f ba = rc_mat34f_mul(b, a);
    rc_vec3f v = rc_vec3f_make(1.0f, 0.0f, 0.0f);

    // a * b is b, then a
    RC_CHECK(rc_mat34f_vec3f_mul(ab, v), ==, rc_mat34f_vec3f_mul(a, rc_mat34f_vec3f_mul(b, v)));
    RC_CHECK(rc_mat34f_vec3f_mul(ab, v), ==, rc_vec3f_make(10.0f, 1.0f, 2.0f));
    RC_CHECK(rc_mat34f_vec3f_mul(ba, v), ==, rc_mat34f_vec3f_mul(b, rc_mat34f_vec3f_mul(a, v)));
    RC_CHECK(rc_mat34f_vec3f_mul(ba, v), ==, rc_vec3f_make(0.0f, 11.0f, 2.0f));
}


RC_TEST(mat34f, mixed_mul)
{
    rc_mat34f translation = rc_mat34f_make_translation(rc_vec3f_make(3.0f, 4.0f, 5.0f));
    rc_mat33f scale = rc_mat33f_make(rc_vec3f_make(2.0f, 0.0f, 0.0f), rc_vec3f_make(0.0f, 3.0f, 0.0f), rc_vec3f_make(0.0f, 0.0f, 4.0f));
    rc_mat34f turned_after = rc_mat33f_mat34f_mul(quarter_turn_z(), translation);
    rc_mat34f scaled_first = rc_mat34f_mat33f_mul(translation, scale);

    // The move, then the turn: (1, 0, 0) goes to (4, 4, 5), then (-4, 4, 5)
    RC_CHECK(rc_mat34f_vec3f_mul(turned_after, rc_vec3f_make(1.0f, 0.0f, 0.0f)), ==, rc_vec3f_make(-4.0f, 4.0f, 5.0f));
    RC_CHECK(turned_after.trans, ==, rc_vec3f_make(-4.0f, 3.0f, 5.0f));

    // The scale, then the move, which leaves the translation alone
    RC_CHECK(scaled_first.trans, ==, rc_vec3f_make(3.0f, 4.0f, 5.0f));
    RC_CHECK(rc_mat34f_vec3f_mul(scaled_first, rc_vec3f_make(1.0f, 1.0f, 1.0f)), ==, rc_vec3f_make(5.0f, 7.0f, 9.0f));
}


RC_TEST(mat34f, lookat_down_the_z_axis)
{
    rc_vec3f eye = rc_vec3f_make(0.0f, 0.0f, 5.0f);
    rc_mat34f view = rc_mat34f_make_lookat(eye, rc_vec3f_make_zero(), rc_vec3f_make_unity());

    // Already looking down -z with y up, so it only moves the eye to the origin
    RC_CHECK(view.rot.cx, ~=, rc_vec3f_make(1.0f, 0.0f, 0.0f));
    RC_CHECK(view.rot.cy, ~=, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(view.rot.cz, ~=, rc_vec3f_make(0.0f, 0.0f, 1.0f));
    RC_CHECK(view.trans, ~=, rc_vec3f_make(0.0f, 0.0f, -5.0f));
    RC_CHECK(rc_mat34f_vec3f_mul(view, eye), ~=, rc_vec3f_make_zero());
    RC_CHECK(rc_mat34f_vec3f_mul(view, rc_vec3f_make_zero()), ~=, rc_vec3f_make(0.0f, 0.0f, -5.0f));
}


RC_TEST(mat34f, lookat_down_the_x_axis)
{
    rc_mat34f view = rc_mat34f_make_lookat(rc_vec3f_make_zero(), rc_vec3f_make_unitx(), rc_vec3f_make_unity());
    rc_mat34f rough_up = rc_mat34f_make_lookat(rc_vec3f_make_zero(), rc_vec3f_make_unitx(), rc_vec3f_make(0.5f, 2.0f, 0.0f));

    // The focus lands on -z, up stays up, and +z is to the right
    RC_CHECK(rc_mat34f_vec3f_mul(view, rc_vec3f_make_unitx()), ~=, rc_vec3f_make(0.0f, 0.0f, -1.0f));
    RC_CHECK(rc_mat34f_vec3f_mul(view, rc_vec3f_make_unity()), ~=, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(rc_mat34f_vec3f_mul(view, rc_vec3f_make_unitz()), ~=, rc_vec3f_make(1.0f, 0.0f, 0.0f));

    // An up that leans towards the focus gives the same view
    RC_CHECK(rough_up.rot.cx, ~=, view.rot.cx);
    RC_CHECK(rough_up.rot.cy, ~=, view.rot.cy);
    RC_CHECK(rough_up.rot.cz, ~=, view.rot.cz);
}


RC_TEST(mat34f, inverse)
{
    rc_mat34f m = rc_mat34f_make(quarter_turn_z(), rc_vec3f_make(2.0f, 3.0f, 4.0f));
    rc_mat34f m_inverse = rc_mat34f_inverse(m);
    rc_mat34f product = rc_mat34f_mul(m, m_inverse);
    rc_mat34f reversed = rc_mat34f_mul(m_inverse, m);
    rc_vec3f v = rc_vec3f_make(4.0f, -1.0f, 2.0f);

    RC_CHECK(product.rot.cx, ==, rc_vec3f_make(1.0f, 0.0f, 0.0f));
    RC_CHECK(product.rot.cy, ==, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(product.rot.cz, ==, rc_vec3f_make(0.0f, 0.0f, 1.0f));
    RC_CHECK(product.trans, ==, rc_vec3f_make_zero());
    RC_CHECK(reversed.rot.cx, ==, rc_vec3f_make(1.0f, 0.0f, 0.0f));
    RC_CHECK(reversed.trans, ==, rc_vec3f_make_zero());
    RC_CHECK(rc_mat34f_vec3f_mul(m_inverse, rc_mat34f_vec3f_mul(m, v)), ==, v);
}


RC_TEST(mat34f, inverse_of_a_scale)
{
    rc_mat33f scale = rc_mat33f_make(rc_vec3f_make(2.0f, 0.0f, 0.0f), rc_vec3f_make(0.0f, 4.0f, 0.0f), rc_vec3f_make(0.0f, 0.0f, 8.0f));
    rc_mat34f m_inverse = rc_mat34f_inverse(rc_mat34f_make(scale, rc_vec3f_make(1.0f, -2.0f, 4.0f)));

    // Undo the move, then the scale
    RC_CHECK(m_inverse.rot.cx, ==, rc_vec3f_make(0.5f, 0.0f, 0.0f));
    RC_CHECK(m_inverse.rot.cy, ==, rc_vec3f_make(0.0f, 0.25f, 0.0f));
    RC_CHECK(m_inverse.rot.cz, ==, rc_vec3f_make(0.0f, 0.0f, 0.125f));
    RC_CHECK(m_inverse.trans, ==, rc_vec3f_make(-0.5f, 0.5f, -0.5f));
}
