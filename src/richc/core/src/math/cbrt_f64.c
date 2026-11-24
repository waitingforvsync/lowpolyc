// math/cbrt_f64.c
//
// rc_cbrt_f64: the real cube root of a double.
//
// A third of |x|'s top 32 bits, plus a bias, read back as a double's top 32 bits, is a first guess
// at the root within 3.2%, and a step of Halley's method takes that to 2^-15.4. We cut that to a
// y of 17 significant bits, so y^3 is exact, and so is x - y^3, so the excess e = x / y^3 - 1
// costs one rounding, and cbrt(x) = y (1 + e)^(1/3) = y (1 + e/3 - e^2/9 + 5e^3/81 - 10e^4/243)
// to within 2^-70, as |e| < 2^-13.1. The correction is tiny beside y, so the error is the final
// rounding and very little more: under 0.501 ulp. Exact cubes come out exact. The guess carries
// x's sign, and every step is odd, so cbrt(-x) is exactly -cbrt(x).
//
// Beyond 2^600 in size or below 2^-600, x is scaled by 2^-600 or 2^600 first, so that nothing
// leaves the normal range. A subnormal x is m 2^-1074 for an integer m, so its root is that of m,
// which we make from the bits, times 2^-358: arithmetic on a subnormal can be slow.

#include "richc/math/math.h"

#include "math/arc.h"

// What the guess adds to a third of |x|'s top 32 bits: the one that makes it closest, relatively.
#define RC_CBRT_BIAS 715093542u


// Whether |x|, as its bits, is strictly between those of 2^-600 and 2^600, so its root needs no
// scaling: one unsigned comparison.
static bool rc_cbrt_is_ordinary(uint64_t magnitude)
{
    return magnitude - 0x1a70000000000001ull < 0x6570000000000000ull - 0x1a70000000000001ull;
}


// The first guess at cbrt(x) for a normal x: a third of |x|'s top 32 bits, plus the bias, as a
// double's top 32 bits, with x's sign.
static double rc_cbrt_guess(double x)
{
    uint64_t bits = rc_bitcast_f64(x);
    uint32_t top = (uint32_t)(bits >> 32) & 0x7fffffffu;

    return rc_bitcast_u64((uint64_t)(top / 3 + RC_CBRT_BIAS) << 32 | (bits & 0x8000000000000000ull));
}


// The cube root of x, for |x| between 2^-600 and 2^600.
static double rc_cbrt_ordinary(double x)
{
    double near = rc_arc_cbrt_halley(x, rc_cbrt_guess(x));
    double y = rc_bitcast_u64(rc_bitcast_f64(near) & 0xfffffff000000000ull);
    double cube = y * y * y;
    double excess = (x - cube) / cube;
    double series = 0x1.5555555555555p-2 +
        excess * (-0x1.c71c71c71c71cp-4 + excess * (0x1.f9add3c0ca458p-5 + excess * -0x1.511e8d2b3183bp-5));

    return y + y * (excess * series);
}


// The cube root of x beyond that, with magnitude its bits less the sign: zero, infinite or NaN;
// subnormal; or scaled into range by 2^600 or 2^-600.
static double rc_cbrt_extreme(double x, uint64_t magnitude)
{
    bool tiny = magnitude < 0x3ff0000000000000ull;

    if (magnitude == 0 || magnitude >= 0x7ff0000000000000ull) {
        return x + x;
    }

    if (magnitude < 0x0010000000000000ull) {
        return rc_cbrt_ordinary(rc_copysign_f64((double)(int64_t)magnitude, x)) * 0x1p-358;
    }

    return rc_cbrt_ordinary(x * (tiny ? 0x1p600 : 0x1p-600)) * (tiny ? 0x1p-200 : 0x1p200);
}


double rc_cbrt_f64(double x)
{
    uint64_t magnitude = rc_bitcast_f64(x) & 0x7fffffffffffffffull;

    if (!rc_cbrt_is_ordinary(magnitude)) {
        return rc_cbrt_extreme(x, magnitude);
    }

    return rc_cbrt_ordinary(x);
}
