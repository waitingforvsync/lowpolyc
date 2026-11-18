// math/mat23f.c
//
// The 2D affine transform operations too big to inline.

#include "richc/math/mat23f.h"

rc_mat23f rc_mat23f_inverse(rc_mat23f m)
{
    rc_mat22f rot = rc_mat22f_inverse(m.rot);

    return (rc_mat23f) {
        .rot = rot,
        .trans = rc_mat22f_vec2f_mul(rot, rc_vec2f_negate(m.trans))
    };
}
