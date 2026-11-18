// math/quatf.c
//
// The quaternion operations too big to inline: converting to and from rotation matrices,
// interpolating, and the exponential and logarithm.

#include "richc/math/quatf.h"

rc_quatf rc_quatf_make_angle_axis(float angle, rc_vec3f axis)
{
    float half = angle * 0.5f;

    return rc_quatf_from_vec3f(rc_vec3f_scalar_mul(rc_vec3f_normalize(axis), rc_sin_f32(half)), rc_cos_f32(half));
}


// The standard expansion of q v q^-1 for each axis v, for a unit q.
rc_mat33f rc_mat33f_from_quatf(rc_quatf q)
{
    float xx = q.xyz.x * q.xyz.x;
    float xy = q.xyz.x * q.xyz.y;
    float xz = q.xyz.x * q.xyz.z;
    float yy = q.xyz.y * q.xyz.y;
    float yz = q.xyz.y * q.xyz.z;
    float zz = q.xyz.z * q.xyz.z;
    float xw = q.xyz.x * q.w;
    float yw = q.xyz.y * q.w;
    float zw = q.xyz.z * q.w;
    float ww = q.w * q.w;

    return (rc_mat33f) {
        {ww + xx - yy - zz, 2.0f * (xy + zw), 2.0f * (xz - yw)},
        {2.0f * (xy - zw), ww - xx + yy - zz, 2.0f * (yz + xw)},
        {2.0f * (xz + yw), 2.0f * (yz - xw), ww - xx - yy + zz}
    };
}


// unscaled divided by 2 sqrt(largest), where largest is 4 c^2 for its largest component c.
static rc_quatf rc_quatf_scaled_by_largest(rc_quatf unscaled, float largest)
{
    return rc_quatf_scalar_mul(unscaled, 0.5f / rc_sqrt_f32(largest));
}


// Mike Day's method ("Converting a Rotation Matrix to a Quaternion", Insomniac Games). Recovering
// each component from its own square root loses the relative signs near a half turn, where w and
// the off-diagonal differences that carry the signs all go to zero. So we find the largest
// component first: four times its square is one of 1 +- m00 +- m11 +- m22, at least 1, and its
// square root is well conditioned. The other three come from off-diagonal sums and differences
// divided by it. Column-major, so m.cx.y is R10 and m.cy.x is R01.
rc_quatf rc_quatf_from_mat33f(rc_mat33f m)
{
    float r00 = m.cx.x;
    float r11 = m.cy.y;
    float r22 = m.cz.z;
    float four_xx = 1.0f + r00 - r11 - r22;
    float four_yy = 1.0f - r00 + r11 - r22;
    float four_zz = 1.0f - r00 - r11 + r22;
    float four_ww = 1.0f + r00 + r11 + r22;

    if (r22 < 0.0f && r00 > r11) {
        return rc_quatf_scaled_by_largest(rc_quatf_make(four_xx, m.cx.y + m.cy.x, m.cx.z + m.cz.x, m.cy.z - m.cz.y), four_xx);
    }

    if (r22 < 0.0f) {
        return rc_quatf_scaled_by_largest(rc_quatf_make(m.cx.y + m.cy.x, four_yy, m.cy.z + m.cz.y, m.cz.x - m.cx.z), four_yy);
    }

    if (r00 < -r11) {
        return rc_quatf_scaled_by_largest(rc_quatf_make(m.cx.z + m.cz.x, m.cy.z + m.cz.y, four_zz, m.cx.y - m.cy.x), four_zz);
    }

    return rc_quatf_scaled_by_largest(rc_quatf_make(m.cy.z - m.cz.y, m.cz.x - m.cx.z, m.cx.y - m.cy.x, four_ww), four_ww);
}


// The slerp between a and b, theta apart, weighting each by the sine of the angle still to go.
static rc_quatf rc_quatf_slerp_arc(rc_quatf a, rc_quatf b, float theta, float t)
{
    float inverse_sin = 1.0f / rc_sin_f32(theta);
    float weight_a = rc_sin_f32((1.0f - t) * theta) * inverse_sin;
    float weight_b = rc_sin_f32(t * theta) * inverse_sin;

    return rc_quatf_add(rc_quatf_scalar_mul(a, weight_a), rc_quatf_scalar_mul(b, weight_b));
}


// The slerp between a and b, whose dot product cos_theta isn't negative. When they are nearly
// the same, sin(theta) vanishes, so we use nlerp, which agrees with slerp to first order.
static rc_quatf rc_quatf_slerp_same_side(rc_quatf a, rc_quatf b, float cos_theta, float t)
{
    if (cos_theta > 0.9995f) {
        return rc_quatf_nlerp(a, b, t);
    }

    return rc_quatf_slerp_arc(a, b, rc_acos_f32(cos_theta), t);
}


// b and -b are the same rotation, so we use whichever is nearer a, to go the short way round.
rc_quatf rc_quatf_slerp(rc_quatf a, rc_quatf b, float t)
{
    float cos_theta = rc_quatf_dot(a, b);

    if (cos_theta < 0.0f) {
        return rc_quatf_slerp_same_side(a, rc_quatf_negate(b), -cos_theta, t);
    }

    return rc_quatf_slerp_same_side(a, b, cos_theta, t);
}


// For q = (v, w), exp(q) = e^w (sin|v| v / |v|, cos|v|). As |v| goes to 0, sin|v| / |v| goes to 1,
// which we use directly rather than divide 0 by 0.
rc_quatf rc_quatf_exp(rc_quatf q)
{
    float vector_length = rc_vec3f_length(q.xyz);
    float exp_w = rc_exp_f32(q.w);
    float scale = vector_length < 1e-6f ? exp_w : exp_w * rc_sin_f32(vector_length) / vector_length;

    return rc_quatf_from_vec3f(rc_vec3f_scalar_mul(q.xyz, scale), exp_w * rc_cos_f32(vector_length));
}


// The log of q, of length length, with a vector part of length vector_length, not near zero. The
// cosine is clamped, since rounding can take it just outside acos's domain.
static rc_quatf rc_quatf_log_turning(rc_quatf q, float length, float vector_length)
{
    float cosine = rc_max_f32(rc_min_f32(q.w / length, 1.0f), -1.0f);
    float scale = rc_acos_f32(cosine) / vector_length;

    return rc_quatf_from_vec3f(rc_vec3f_scalar_mul(q.xyz, scale), rc_log_f32(length));
}


// For q = (v, w) of length |q|, log(q) = (acos(w / |q|) v / |v|, ln|q|). With no vector part to
// speak of, the vector part of the log is zero too.
rc_quatf rc_quatf_log(rc_quatf q)
{
    float length = rc_quatf_length(q);
    float vector_length = rc_vec3f_length(q.xyz);

    RC_ASSERT(length != 0.0f);
    if (vector_length < 1e-6f) {
        return rc_quatf_from_vec3f(rc_vec3f_make_zero(), rc_log_f32(length));
    }

    return rc_quatf_log_turning(q, length, vector_length);
}


rc_quatf rc_quatf_pow(rc_quatf q, float t)
{
    return rc_quatf_exp(rc_quatf_scalar_mul(rc_quatf_log(q), t));
}
