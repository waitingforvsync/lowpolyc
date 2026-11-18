// math/mat33f.c
//
// The 3x3 matrix operations too big to inline.

#include "richc/math/mat33f.h"

rc_mat33f rc_mat33f_make_rotation_x(float angle)
{
    float sine = rc_sin_f32(angle);
    float cosine = rc_cos_f32(angle);

    return (rc_mat33f) {
        {1.0f, 0.0f, 0.0f},
        {0.0f, cosine, sine},
        {0.0f, -sine, cosine}
    };
}


rc_mat33f rc_mat33f_make_rotation_y(float angle)
{
    float sine = rc_sin_f32(angle);
    float cosine = rc_cos_f32(angle);

    return (rc_mat33f) {
        {cosine, 0.0f, -sine},
        {0.0f, 1.0f, 0.0f},
        {sine, 0.0f, cosine}
    };
}


rc_mat33f rc_mat33f_make_rotation_z(float angle)
{
    float sine = rc_sin_f32(angle);
    float cosine = rc_cos_f32(angle);

    return (rc_mat33f) {
        {cosine, sine, 0.0f},
        {-sine, cosine, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };
}


rc_mat33f rc_mat33f_mul(rc_mat33f a, rc_mat33f b)
{
    return (rc_mat33f) {
        rc_mat33f_vec3f_mul(a, b.cx),
        rc_mat33f_vec3f_mul(a, b.cy),
        rc_mat33f_vec3f_mul(a, b.cz)
    };
}


rc_mat33f rc_mat33f_inverse(rc_mat33f m)
{
    rc_vec3f row_x = rc_vec3f_cross(m.cy, m.cz);
    rc_vec3f row_y = rc_vec3f_cross(m.cz, m.cx);
    rc_vec3f row_z = rc_vec3f_cross(m.cx, m.cy);
    float determinant = rc_vec3f_dot(m.cx, row_x);

    RC_ASSERT(determinant != 0.0f);
    return rc_mat33f_scalar_mul(rc_mat33f_make_transpose(row_x, row_y, row_z), 1.0f / determinant);
}
