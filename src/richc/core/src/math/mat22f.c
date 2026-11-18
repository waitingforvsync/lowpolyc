// math/mat22f.c
//
// The 2x2 matrix operations too big to inline.

#include "richc/math/mat22f.h"

rc_mat22f rc_mat22f_make_rotation(float angle)
{
    float sine = rc_sin_f32(angle);
    float cosine = rc_cos_f32(angle);

    return (rc_mat22f) {
        {cosine, sine},
        {-sine, cosine}
    };
}


rc_mat22f rc_mat22f_inverse(rc_mat22f m)
{
    float determinant = rc_mat22f_determinant(m);
    float scale = 1.0f / determinant;

    RC_ASSERT(determinant != 0.0f);
    return (rc_mat22f) {
        {m.cy.y * scale, -m.cx.y * scale},
        {-m.cy.x * scale, m.cx.x * scale}
    };
}
