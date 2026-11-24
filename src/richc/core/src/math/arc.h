// math/arc.h
//
// What the inverse trigonometric functions and cbrt share: pi and pi/2 to twice double precision,
// the table of arctangents that atan_f64 and atan2_f64 reduce against, signs as factors, a little
// exact arithmetic, the kernels, inlined into each function, and a step of Halley's method.
// Private to core.
//
// The kernels give the part of asin or atan beyond its first term, or as a ratio to it, so callers
// can add that term last and round once. The f32 kernels are evaluated in double, which leaves
// them more accurate than a float needs. The polynomials were fitted with Sollya's fpminimax and
// the rational function by our own Remez-like iteration in mpmath, both minimising the relative
// error over double coefficients, and every constant worked out with mpmath.

#ifndef RC_MATH_ARC_H_
#define RC_MATH_ARC_H_

#include "richc/math/math.h"

// pi/2 and pi, each the double nearest it plus the double nearest what that leaves, and the
// doubles nearest pi/4 and 3pi/4.
#define RC_ARC_PI_2_HI 0x1.921fb54442d18p0
#define RC_ARC_PI_2_LO 0x1.1a62633145c07p-54
#define RC_ARC_PI_HI   0x1.921fb54442d18p1
#define RC_ARC_PI_LO   0x1.1a62633145c07p-53
#define RC_ARC_PI_4    0x1.921fb54442d18p-1
#define RC_ARC_PI_3_4  0x1.2d97c7f3321d2p1

// The double nearest tan(pi/8), where atan2_f32 changes reduction, and the bits of the largest
// floats below tan(pi/8) and tan(3pi/8), where atan_f32 does, testing |x|'s bits.
#define RC_ARC_TAN_PI_8 0x1.a827999fcef32p-2

enum {
    RC_ARC_TAN_PI_8_F32_BITS = 0x3ed413cc,
    RC_ARC_TAN_3PI_8_F32_BITS = 0x401a8279
};

// The atan table spans [1/16, 16): 4 cells in each binade, indexed by the top 14 bits of a double,
// the sign, the exponent and two bits of the significand.
enum {
    RC_ARC_ATAN_TABLE_SHIFT = 50,
    RC_ARC_ATAN_TABLE_FIRST = 0xfec,
    RC_ARC_ATAN_TABLE_LAST = 0x100b,
    RC_ARC_ATAN_TABLE_COUNT = 32
};

// A value as the unevaluated sum of a double and a much smaller correction.
typedef struct rc_arc_sum {
    double hi;
    double lo;
} rc_arc_sum;

// atan(c) for the middle c of each cell, c = 2^e (1 + (k + 1/2) / 4) at index 4 (e + 4) + k.
extern const rc_arc_sum rc_arc_atan_table[RC_ARC_ATAN_TABLE_COUNT];

// The middle of the cell of the atan table that holds a double with the given top 14 bits.
static inline double rc_arc_atan_cell_middle(uint64_t cell)
{
    return rc_bitcast_u64(cell << RC_ARC_ATAN_TABLE_SHIFT | 1ull << (RC_ARC_ATAN_TABLE_SHIFT - 1));
}

// 1 or -1 for a sign bit of 0 or 1. Multiplying by it is how we give a result a sign: it costs
// about what copysign does natively, and much less under wasm. It leaves a NaN's sign alone, so
// where a NaN must follow x's sign, copysign still does that.
static inline double rc_arc_sign_f64(uint64_t sign_bit)
{
    static const double signs[2] = {1.0, -1.0};

    return signs[sign_bit];
}

// ---- exact arithmetic ----

// a + b exactly, for |a| >= |b| or a = 0.
static inline rc_arc_sum rc_arc_fast_two_sum(double a, double b)
{
    double hi = a + b;

    return (rc_arc_sum) {.hi = hi, .lo = b - (hi - a)};
}


// x cut to its top 26 significant bits, so the product of two such is exact.
static inline double rc_arc_top_bits(double x)
{
    return rc_bitcast_u64(rc_bitcast_f64(x) & 0xfffffffff8000000ull);
}


// c - a b, for c close to a b. Built from the products of the halves of a and b, all but the
// smallest exact, it errs by around 2^-100 of a b, where a rounded a b would err by 2^-53.
static inline double rc_arc_residual_f64(double c, double a, double b)
{
    double a_hi = rc_arc_top_bits(a);
    double a_lo = a - a_hi;
    double b_hi = rc_arc_top_bits(b);
    double b_lo = b - b_hi;
    double product = a * b;
    double product_lo = ((a_hi * b_hi - product) + a_hi * b_lo + a_lo * b_hi) + a_lo * b_lo;

    return (c - product) - product_lo;
}

// ---- f64 kernels ----

// atan(u) - u, for |u| <= 1/16, as u^3 times a quartic in u^2, by Estrin's scheme.
// Fitted error 2^-62.5 relative to atan(u).
static inline double rc_arc_atan_tail_f64(double u)
{
    double s = u * u;
    double s2 = s * s;
    double p0 = -0x1.5555555555537p-2 + s * 0x1.99999999637fdp-3;
    double p1 = -0x1.2492482a8ef26p-3 + s * 0x1.c718822e83e7p-4;
    double q = p0 + s2 * (p1 + s2 * -0x1.70c30efcd41fbp-4);

    return (u * s) * q;
}


// asin(a) / a - 1, for |a| <= 1/2, given z = a^2: z N(z) / D(z) for N a quintic and D a quartic,
// each by Horner's scheme. Fitted error 2^-62.6 relative to asin(a), 2^-59.6 with the coefficients
// rounded to doubles; the roundings in it cost more, but they are scaled by the ratio, which is at
// most 0.05.
static inline double rc_arc_asin_ratio_f64(double z)
{
    double n = 0x1.5555555555553p-3 + z * (-0x1.4ea19dfd137d2p-2 + z * (0x1.9fa466b750289p-3 +
        z * (-0x1.4d30756d848f3p-5 + z * (0x1.aaefad301c1d2p-11 + z * 0x1.309290a0b148ap-15))));
    double d = 1.0 + z * (-0x1.3492d0176842ap1 + z * (0x1.04702c6366761p1 + z * (-0x1.64741e33f88b3p-1 + z * 0x1.411cb5e82614fp-4)));

    return z * n / d;
}

// ---- f32 kernels, in double ----

// atan(u) / u - 1, for |u| <= 0.41422, a little past tan(pi/8), as u^2 times a quartic in u^2, by
// Estrin's scheme. Fitted error 2^-30.5 relative to atan(u), which costs a float at most 0.02 ulp.
static inline double rc_arc_atan_ratio_f32(double u)
{
    double s = u * u;
    double s2 = s * s;
    double s4 = s2 * s2;
    double p0 = -0x1.55554927a655dp-2 + s * 0x1.999195df62dd2p-3;
    double p1 = -0x1.23b51e343f8f3p-3 + s * 0x1.b1ebe7bc472f7p-4;

    return s * ((p0 + s2 * p1) + s4 * -0x1.f1ec096aa3c41p-5);
}


// asin(a) - a, for |a| <= 1/2 and square = a^2, as a^3 times a quintic in a^2, by Estrin's
// scheme. Fitted error 2^-31.8 relative to asin(a), which costs a float at most 0.01 ulp.
static inline double rc_arc_asin_tail_f32(double a, double square)
{
    double s = square;
    double s2 = s * s;
    double p0 = 0x1.55554c5fb0496p-3 + s * 0x1.33387c9d34d7dp-4;
    double p1 = 0x1.6caed21567fd6p-5 + s * 0x1.0491a13d1fca6p-5;
    double p2 = 0x1.d91fa335d62fap-7 + s * 0x1.3355c1d8ae1a5p-5;
    double q = p0 + s2 * (p1 + s2 * p2);

    return a * (s * q);
}

// ---- cube root ----

// A step of Halley's method for cbrt(x), y (2x + y^3) / (x + 2y^3), which triples the bits of y
// that are right. Every operation in it is odd, so it keeps cbrt's symmetry exactly.
static inline double rc_arc_cbrt_halley(double x, double y)
{
    double cube = y * y * y;

    return y * (x + x + cube) / (x + cube + cube);
}

#endif // RC_MATH_ARC_H_
