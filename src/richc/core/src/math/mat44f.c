// math/mat44f.c
//
// The 4x4 matrix operations too big to inline.

#include "richc/math/mat44f.h"

// The 24 terms of the Leibniz expansion, one for each permutation of the four columns.
float rc_mat44f_determinant(rc_mat44f m)
{
    return m.cx.w * m.cy.z * m.cz.y * m.cw.x - m.cx.z * m.cy.w * m.cz.y * m.cw.x -
        m.cx.w * m.cy.y * m.cz.z * m.cw.x + m.cx.y * m.cy.w * m.cz.z * m.cw.x +
        m.cx.z * m.cy.y * m.cz.w * m.cw.x - m.cx.y * m.cy.z * m.cz.w * m.cw.x -
        m.cx.w * m.cy.z * m.cz.x * m.cw.y + m.cx.z * m.cy.w * m.cz.x * m.cw.y +
        m.cx.w * m.cy.x * m.cz.z * m.cw.y - m.cx.x * m.cy.w * m.cz.z * m.cw.y -
        m.cx.z * m.cy.x * m.cz.w * m.cw.y + m.cx.x * m.cy.z * m.cz.w * m.cw.y +
        m.cx.w * m.cy.y * m.cz.x * m.cw.z - m.cx.y * m.cy.w * m.cz.x * m.cw.z -
        m.cx.w * m.cy.x * m.cz.y * m.cw.z + m.cx.x * m.cy.w * m.cz.y * m.cw.z +
        m.cx.y * m.cy.x * m.cz.w * m.cw.z - m.cx.x * m.cy.y * m.cz.w * m.cw.z -
        m.cx.z * m.cy.y * m.cz.x * m.cw.w + m.cx.y * m.cy.z * m.cz.x * m.cw.w +
        m.cx.z * m.cy.x * m.cz.y * m.cw.w - m.cx.x * m.cy.z * m.cz.y * m.cw.w -
        m.cx.y * m.cy.x * m.cz.z * m.cw.w + m.cx.x * m.cy.y * m.cz.z * m.cw.w;
}


// By 2x2 determinants: s0 to s5 are the six from the first two columns, and c0 to c5 the six from
// the last two, each pairing two rows. The determinant is a sum of products of the two, and each
// element of the inverse is a cofactor built from them.
rc_mat44f rc_mat44f_inverse(rc_mat44f m)
{
    float s0 = m.cx.x * m.cy.y - m.cy.x * m.cx.y;
    float s1 = m.cx.x * m.cy.z - m.cy.x * m.cx.z;
    float s2 = m.cx.x * m.cy.w - m.cy.x * m.cx.w;
    float s3 = m.cx.y * m.cy.z - m.cy.y * m.cx.z;
    float s4 = m.cx.y * m.cy.w - m.cy.y * m.cx.w;
    float s5 = m.cx.z * m.cy.w - m.cy.z * m.cx.w;
    float c0 = m.cz.x * m.cw.y - m.cw.x * m.cz.y;
    float c1 = m.cz.x * m.cw.z - m.cw.x * m.cz.z;
    float c2 = m.cz.x * m.cw.w - m.cw.x * m.cz.w;
    float c3 = m.cz.y * m.cw.z - m.cw.y * m.cz.z;
    float c4 = m.cz.y * m.cw.w - m.cw.y * m.cz.w;
    float c5 = m.cz.z * m.cw.w - m.cw.z * m.cz.w;
    float determinant = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
    float scale = 1.0f / determinant;

    RC_ASSERT(determinant != 0.0f);
    return (rc_mat44f) {
        {
            (m.cy.y * c5 - m.cy.z * c4 + m.cy.w * c3) * scale,
            (-m.cx.y * c5 + m.cx.z * c4 - m.cx.w * c3) * scale,
            (m.cw.y * s5 - m.cw.z * s4 + m.cw.w * s3) * scale,
            (-m.cz.y * s5 + m.cz.z * s4 - m.cz.w * s3) * scale
        },
        {
            (-m.cy.x * c5 + m.cy.z * c2 - m.cy.w * c1) * scale,
            (m.cx.x * c5 - m.cx.z * c2 + m.cx.w * c1) * scale,
            (-m.cw.x * s5 + m.cw.z * s2 - m.cw.w * s1) * scale,
            (m.cz.x * s5 - m.cz.z * s2 + m.cz.w * s1) * scale
        },
        {
            (m.cy.x * c4 - m.cy.y * c2 + m.cy.w * c0) * scale,
            (-m.cx.x * c4 + m.cx.y * c2 - m.cx.w * c0) * scale,
            (m.cw.x * s4 - m.cw.y * s2 + m.cw.w * s0) * scale,
            (-m.cz.x * s4 + m.cz.y * s2 - m.cz.w * s0) * scale
        },
        {
            (-m.cy.x * c3 + m.cy.y * c1 - m.cy.z * c0) * scale,
            (m.cx.x * c3 - m.cx.y * c1 + m.cx.z * c0) * scale,
            (-m.cw.x * s3 + m.cw.y * s1 - m.cw.z * s0) * scale,
            (m.cz.x * s3 - m.cz.y * s1 + m.cz.z * s0) * scale
        }
    };
}


rc_mat44f rc_mat44f_make_ortho(float left, float right, float top, float bottom, float z_near, float z_far)
{
    float width = right - left;
    float height = top - bottom;
    float depth = z_far - z_near;

    return (rc_mat44f) {
        rc_vec4f_scalar_mul(rc_vec4f_make_unitx(), 2.0f / width),
        rc_vec4f_scalar_mul(rc_vec4f_make_unity(), 2.0f / height),
        rc_vec4f_scalar_mul(rc_vec4f_make_unitz(), 1.0f / depth),
        {-(right + left) / width, -(top + bottom) / height, z_far / depth, 1.0f}
    };
}


rc_mat44f rc_mat44f_mul(rc_mat44f a, rc_mat44f b)
{
    return (rc_mat44f) {
        rc_mat44f_vec4f_mul(a, b.cx),
        rc_mat44f_vec4f_mul(a, b.cy),
        rc_mat44f_vec4f_mul(a, b.cz),
        rc_mat44f_vec4f_mul(a, b.cw)
    };
}


rc_mat44f rc_mat44f_make_perspective(float y_fov, float aspect, float z_near, float z_far)
{
    float focal = 1.0f / rc_tan_f32(y_fov / 2.0f);

    return (rc_mat44f) {
        {focal / aspect, 0.0f, 0.0f, 0.0f},
        {0.0f, focal, 0.0f, 0.0f},
        {0.0f, 0.0f, z_near / (z_far - z_near), -1.0f},
        {0.0f, 0.0f, z_far * z_near / (z_far - z_near), 0.0f}
    };
}


rc_mat44f rc_mat44f_make_perspective_inf(float y_fov, float aspect, float z_near)
{
    float focal = 1.0f / rc_tan_f32(y_fov / 2.0f);

    return (rc_mat44f) {
        {focal / aspect, 0.0f, 0.0f, 0.0f},
        {0.0f, focal, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, -1.0f},
        {0.0f, 0.0f, z_near, 0.0f}
    };
}
