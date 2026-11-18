// math/mat34f.c
//
// The 3D affine transform operations too big to inline.

#include "richc/math/mat34f.h"

rc_mat34f rc_mat34f_make_lookat(rc_vec3f eye, rc_vec3f focus, rc_vec3f up)
{
    rc_vec3f forward = rc_vec3f_normalize(rc_vec3f_sub(focus, eye));
    rc_vec3f side = rc_vec3f_normalize(rc_vec3f_cross(forward, up));
    rc_vec3f true_up = rc_vec3f_cross(side, forward);
    rc_mat33f rot = rc_mat33f_make_transpose(side, true_up, rc_vec3f_negate(forward));

    return (rc_mat34f) {
        .rot = rot,
        .trans = rc_mat33f_vec3f_mul(rot, rc_vec3f_negate(eye))
    };
}


rc_mat34f rc_mat34f_inverse(rc_mat34f m)
{
    rc_mat33f rot = rc_mat33f_inverse(m.rot);

    return (rc_mat34f) {
        .rot = rot,
        .trans = rc_mat33f_vec3f_mul(rot, rc_vec3f_negate(m.trans))
    };
}
