// math/cbrt_f32.c
//
// rc_cbrt_f32: the real cube root of a float, worked in double.
//
// A third of |x|'s bits, plus a bias, read back as a float, is a first guess at the root within
// 3.2%: it divides the exponent by 3 and roughly roots the rest. Each step of Halley's method, in
// double, triples the bits that are right, to 2^-15.4 and then about 2^-45, so the result rounds
// to the nearest float but in the rarest of cases: at worst 0.5 ulp and a hair more, over every
// float. Exact cubes come out exact. The guess carries x's sign, and every step is odd, so
// cbrt(-x) is exactly -cbrt(x). A subnormal x is m 2^-149 for an integer m, so its root is that of
// 2m, a normal float we make from the bits, times 2^-50: arithmetic on a subnormal can be slow.

#include "richc/math/math.h"

#include "math/arc.h"

// What the guess adds to a third of |x|'s bits: the one that makes it closest, relatively.
#define RC_CBRT_BIAS 709953151u


// Whether |x|, as its bits, is a normal float: not zero, subnormal, infinite or NaN.
static bool rc_cbrt_is_normal(uint32_t magnitude)
{
    return magnitude - 0x00800000u < 0x7f000000u;
}


// The cube root of a normal float x, unrounded.
static double rc_cbrt_normal(float x)
{
    uint32_t bits = rc_bitcast_f32(x);
    uint32_t magnitude = bits & 0x7fffffffu;
    double guess = (double)rc_bitcast_u32((magnitude / 3 + RC_CBRT_BIAS) | (bits & 0x80000000u));
    double d = (double)x;

    return rc_arc_cbrt_halley(d, rc_arc_cbrt_halley(d, guess));
}


// The cube root of a float that isn't normal, with magnitude its bits less the sign: zero,
// infinite or NaN, or subnormal, as 2m times 2^-150.
static float rc_cbrt_abnormal(float x, uint32_t magnitude)
{
    if (magnitude == 0 || magnitude >= 0x7f800000u) {
        return x + x;
    }

    return (float)(rc_cbrt_normal(rc_copysign_f32((float)(magnitude << 1), x)) * 0x1p-50);
}


float rc_cbrt_f32(float x)
{
    uint32_t magnitude = rc_bitcast_f32(x) & 0x7fffffffu;

    if (!rc_cbrt_is_normal(magnitude)) {
        return rc_cbrt_abnormal(x, magnitude);
    }

    return (float)rc_cbrt_normal(x);
}
