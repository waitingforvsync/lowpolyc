// test_mat23f.c
//
// Tests for rc_mat23f, the 2D affine transform.

#include "richc/math/mat23f.h"

#include "richc/test.h"


// The anticlockwise quarter turn, whose entries are exact.
static rc_mat22f quarter_turn(void)
{
    return rc_mat22f_make(rc_vec2f_make(0.0f, 1.0f), rc_vec2f_make(-1.0f, 0.0f));
}


RC_TEST(mat23f, make)
{
    float elements[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    rc_mat22f rot = rc_mat22f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.0f, 4.0f));
    rc_mat23f m = rc_mat23f_make(rot, rc_vec2f_make(5.0f, 6.0f));
    rc_mat23f identity = rc_mat23f_make_identity();
    rc_mat23f translation = rc_mat23f_make_translation(rc_vec2f_make(7.0f, 8.0f));
    rc_mat23f linear = rc_mat23f_from_mat22f(rot);
    rc_mat23f from_floats = rc_mat23f_from_floats(elements);

    RC_CHECK(m.rot.cx, ==, rc_vec2f_make(1.0f, 2.0f));
    RC_CHECK(m.rot.cy, ==, rc_vec2f_make(3.0f, 4.0f));
    RC_CHECK(m.trans, ==, rc_vec2f_make(5.0f, 6.0f));
    RC_CHECK(identity.rot.cx, ==, rc_vec2f_make(1.0f, 0.0f));
    RC_CHECK(identity.rot.cy, ==, rc_vec2f_make(0.0f, 1.0f));
    RC_CHECK(identity.trans, ==, rc_vec2f_make_zero());
    RC_CHECK(translation.rot.cx, ==, rc_vec2f_make(1.0f, 0.0f));
    RC_CHECK(translation.rot.cy, ==, rc_vec2f_make(0.0f, 1.0f));
    RC_CHECK(translation.trans, ==, rc_vec2f_make(7.0f, 8.0f));
    RC_CHECK(linear.rot.cx, ==, rot.cx);
    RC_CHECK(linear.rot.cy, ==, rot.cy);
    RC_CHECK(linear.trans, ==, rc_vec2f_make_zero());

    // Column by column, the translation last, both ways
    RC_CHECK(from_floats.rot.cx, ==, rc_vec2f_make(1.0f, 2.0f));
    RC_CHECK(from_floats.rot.cy, ==, rc_vec2f_make(3.0f, 4.0f));
    RC_CHECK(from_floats.trans, ==, rc_vec2f_make(5.0f, 6.0f));
    RC_CHECK_TRUE(rc_mat23f_as_floats(&m) == &m.rot.cx.x);
    RC_CHECK(rc_mat23f_as_floats(&m)[4], ==, 5.0f);
}


RC_TEST(mat23f, vec2f_mul)
{
    rc_mat23f translation = rc_mat23f_make_translation(rc_vec2f_make(5.0f, 6.0f));
    rc_mat23f turn_then_move = rc_mat23f_make(quarter_turn(), rc_vec2f_make(1.0f, 0.0f));

    RC_CHECK(rc_mat23f_vec2f_mul(translation, rc_vec2f_make(1.0f, 2.0f)), ==, rc_vec2f_make(6.0f, 8.0f));
    RC_CHECK(rc_mat23f_vec2f_mul(rc_mat23f_make_identity(), rc_vec2f_make(1.0f, 2.0f)), ==, rc_vec2f_make(1.0f, 2.0f));

    // The quarter turn takes (1, 0) to (0, 1), and the move to (1, 1)
    RC_CHECK(rc_mat23f_vec2f_mul(turn_then_move, rc_vec2f_make(1.0f, 0.0f)), ==, rc_vec2f_make(1.0f, 1.0f));
    RC_CHECK(rc_mat23f_vec2f_mul(turn_then_move, rc_vec2f_make_zero()), ==, rc_vec2f_make(1.0f, 0.0f));
}


RC_TEST(mat23f, mul)
{
    rc_mat23f a = rc_mat23f_make_translation(rc_vec2f_make(10.0f, 0.0f));
    rc_mat23f b = rc_mat23f_make(quarter_turn(), rc_vec2f_make(0.0f, 2.0f));
    rc_mat23f ab = rc_mat23f_mul(a, b);
    rc_mat23f ba = rc_mat23f_mul(b, a);
    rc_vec2f v = rc_vec2f_make(1.0f, 0.0f);

    // a * b is b, then a
    RC_CHECK(rc_mat23f_vec2f_mul(ab, v), ==, rc_mat23f_vec2f_mul(a, rc_mat23f_vec2f_mul(b, v)));
    RC_CHECK(rc_mat23f_vec2f_mul(ab, v), ==, rc_vec2f_make(10.0f, 3.0f));
    RC_CHECK(rc_mat23f_vec2f_mul(ba, v), ==, rc_mat23f_vec2f_mul(b, rc_mat23f_vec2f_mul(a, v)));
    RC_CHECK(rc_mat23f_vec2f_mul(ba, v), ==, rc_vec2f_make(0.0f, 13.0f));
}


RC_TEST(mat23f, mixed_mul)
{
    rc_mat22f quarter = quarter_turn();
    rc_mat23f translation = rc_mat23f_make_translation(rc_vec2f_make(3.0f, 4.0f));
    rc_mat22f scale = rc_mat22f_make(rc_vec2f_make(2.0f, 0.0f), rc_vec2f_make(0.0f, 3.0f));
    rc_mat23f turned_after = rc_mat22f_mat23f_mul(quarter, translation);
    rc_mat23f scaled_first = rc_mat23f_mat22f_mul(translation, scale);

    // The move, then the turn: (1, 0) goes to (4, 4), then (-4, 4)
    RC_CHECK(rc_mat23f_vec2f_mul(turned_after, rc_vec2f_make(1.0f, 0.0f)), ==, rc_vec2f_make(-4.0f, 4.0f));
    RC_CHECK(turned_after.trans, ==, rc_vec2f_make(-4.0f, 3.0f));

    // The scale, then the move, which leaves the translation alone
    RC_CHECK(scaled_first.trans, ==, rc_vec2f_make(3.0f, 4.0f));
    RC_CHECK(rc_mat23f_vec2f_mul(scaled_first, rc_vec2f_make(1.0f, 1.0f)), ==, rc_vec2f_make(5.0f, 7.0f));
}


RC_TEST(mat23f, inverse)
{
    rc_mat23f m = rc_mat23f_make(quarter_turn(), rc_vec2f_make(2.0f, 3.0f));
    rc_mat23f m_inverse = rc_mat23f_inverse(m);
    rc_mat23f product = rc_mat23f_mul(m, m_inverse);
    rc_mat23f reversed = rc_mat23f_mul(m_inverse, m);
    rc_vec2f v = rc_vec2f_make(4.0f, -1.0f);

    RC_CHECK(product.rot.cx, ==, rc_vec2f_make(1.0f, 0.0f));
    RC_CHECK(product.rot.cy, ==, rc_vec2f_make(0.0f, 1.0f));
    RC_CHECK(product.trans, ==, rc_vec2f_make_zero());
    RC_CHECK(reversed.rot.cx, ==, rc_vec2f_make(1.0f, 0.0f));
    RC_CHECK(reversed.rot.cy, ==, rc_vec2f_make(0.0f, 1.0f));
    RC_CHECK(reversed.trans, ==, rc_vec2f_make_zero());
    RC_CHECK(rc_mat23f_vec2f_mul(m_inverse, rc_mat23f_vec2f_mul(m, v)), ==, v);
}


RC_TEST(mat23f, inverse_of_a_scale)
{
    rc_mat22f scale = rc_mat22f_make(rc_vec2f_make(2.0f, 0.0f), rc_vec2f_make(0.0f, 4.0f));
    rc_mat23f m = rc_mat23f_make(scale, rc_vec2f_make(1.0f, -2.0f));
    rc_mat23f m_inverse = rc_mat23f_inverse(m);

    // Undo the move, then the scale
    RC_CHECK(m_inverse.rot.cx, ==, rc_vec2f_make(0.5f, 0.0f));
    RC_CHECK(m_inverse.rot.cy, ==, rc_vec2f_make(0.0f, 0.25f));
    RC_CHECK(m_inverse.trans, ==, rc_vec2f_make(-0.5f, 0.5f));
}
