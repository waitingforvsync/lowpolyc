// math/log_f32.c
//
// The natural log of a float, worked in double from the split of exp_log_f32.h: with x = 2^k m,
// and m x scale = 1 + r, log(x) = (k - log2(scale)) ln(2) + log(1 + r). log(1 + r) comes from a
// quartic, r + a2 r^2 + a3 r^3 + a4 r^4: Sollya's fpminimax, relative error 2^-36.3, which adds
// under 0.001 ulp to the rounding to float. log(1) is exactly +0. Zeros, subnormals, negatives,
// infinities and NaNs all fail one unsigned comparison on the bits, and take the slow path.

#include "richc/math/math.h"

#include "math/exp_log_f32.h"

// base_ln + log(1 + r) for r in [-0.003876, 0.00390625]. Split as (base_ln + r + a2 r^2) +
// r^3 (a3 + a4 r), the two halves are ready together.
static double rc_log_f32_poly(double base_ln, double r)
{
    double a2 = rc_log_f32_coefficients[0];
    double a3 = rc_log_f32_coefficients[1];
    double a4 = rc_log_f32_coefficients[2];
    double r2 = r * r;
    double r3 = r2 * r;

    return ((base_ln + r) + a2 * r2) + r3 * (a3 + a4 * r);
}


// The natural log of a float, from the bits of a positive normal one or of a scaled subnormal.
static double rc_log_f32_kernel(uint32_t bits)
{
    rc_log_f32_parts parts = rc_log_f32_split(bits);

    return rc_log_f32_poly(parts.base * 0x1.62e42fefa39efp-1, parts.r);
}


// The natural log of a float that isn't positive and normal: -inf for a zero, +inf for +inf, and
// NaN for the negatives and NaN. A subnormal is scaled up for the kernel, and is checked for first
// on the bits, as the only one of these that needs real work.
static float rc_log_f32_special(float x)
{
    uint32_t bits = rc_bitcast_f32(x);

    if (bits - 1u < 0x007fffffu) {
        return (float)rc_log_f32_kernel(rc_log_f32_subnormal_bits(bits));
    }

    if (x == 0.0f) {
        return rc_bitcast_u32(0xff800000u);
    }

    return x > 0.0f ? x : rc_bitcast_u32(0x7fc00000u);
}


float rc_log_f32(float x)
{
    uint32_t bits = rc_bitcast_f32(x);

    // The main path first, in a ternary: so gcc and clang (wasm too) both let it fall through
    return !rc_log_f32_is_special(bits) ? (float)rc_log_f32_kernel(bits) : rc_log_f32_special(x);
}
