// math/log2_f32.c
//
// The base 2 log of a float, from the log2 kernel in exp_log_f32.h, which works in double. Its
// error of 2^-36.3 relative adds under 0.001 ulp to the rounding to float. A power of 2, 2^k,
// has a mantissa of exactly 1 and a log2 of exactly k. Zeros, subnormals, negatives, infinities
// and NaNs all fail one unsigned comparison on the bits, and take the slow path.

#include "richc/math/math.h"

#include "math/exp_log_f32.h"

// The base 2 log of a float that isn't positive and normal: -inf for a zero, +inf for +inf, and
// NaN for the negatives and NaN. A subnormal is scaled up for the kernel, and is checked for first
// on the bits, as the only one of these that needs real work.
static float rc_log2_f32_special(float x)
{
    uint32_t bits = rc_bitcast_f32(x);

    if (bits - 1u < 0x007fffffu) {
        return (float)rc_log2_f32_kernel(rc_log_f32_subnormal_bits(bits));
    }

    if (x == 0.0f) {
        return rc_bitcast_u32(0xff800000u);
    }

    return x > 0.0f ? x : rc_bitcast_u32(0x7fc00000u);
}


float rc_log2_f32(float x)
{
    uint32_t bits = rc_bitcast_f32(x);

    // The main path first, in a ternary: so gcc and clang (wasm too) both let it fall through
    return !rc_log_f32_is_special(bits) ? (float)rc_log2_f32_kernel(bits) : rc_log2_f32_special(x);
}
