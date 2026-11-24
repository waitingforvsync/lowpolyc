// math/trig.h
//
// The parts sin, cos and tan share: reducing an angle to within pi/4 of a multiple of pi/2, and
// the kernels that finish the job on [-pi/4, pi/4]. Private to src/math.
//
// Reduction takes k, the nearest whole number to x 2/pi, and r = x - k pi/2. Below 2^20 that is
// Cody-Waite: pi/2 is split into pieces short enough that k times each is exact, so only the last,
// tiny product rounds. Floats need two pieces there, doubles four. Above, the bits of 2/pi are
// worked through in integer arithmetic (Payne-Hanek): out of line for doubles (trig_reduce.c), and
// inline for floats, where two 64-bit products do. A double comes no closer than 2^-60.9 to a
// multiple of pi/2, and a float no closer than 2^-29.2 (2^-27.8 below 2^20;
// tools/math/trig/hardest.py searched every binade), so r is good to a relative 2^-64 for a double
// and 2^-39 for a float.
//
// Each kernel gives sin(r + q pi/2) or tan(r + q pi/2) for the quadrant q in one pass, without a
// branch: tables indexed by q choose sine's or cosine's polynomial and the sign, so no time goes on
// the one not wanted. The tables are static, so a caller that knows q (a double below pi/4, which
// needs no reducing) gets the constants folded in. sin and cos were fitted by fpminimax in Sollya
// (tools/math/trig/fit_trig.sollya) and tan's rationals by our own Remez in mpmath and Sollya
// (fit_tan.py), relative to the function on [0, 0.7854], a hair past pi/4.

#ifndef RC_MATH_TRIG_H_
#define RC_MATH_TRIG_H_

#include "richc/math/math.h"

// The short reductions are exact enough below 2^20, here as bit patterns. Doubles below pi/4 need
// no reducing: the last are the bits of pi/4's double, a hair below pi/4.
#define RC_TRIG_SHORT_LIMIT_BITS_F64 0x4130000000000000ull
#define RC_TRIG_SHORT_LIMIT_BITS_F32 0x49800000u
#define RC_TRIG_QUARTER_PI_BITS_F64  0x3fe921fb54442d18ull

// Below these, as bit patterns, the result rounds to x for sine and tangent and to 1 for cosine,
// so it is given at once. A double's sine is x below 2^-26, where x^3/6 is under a third of x's
// last place; tangent's x^3/3 and cosine's x^2/2 need 2^-27. Floats need 2^-12, where cosine's
// x^2/2 is under 2^-25, half the last place below 1.
#define RC_TRIG_SIN_TINY_BITS_F64 0x3e50000000000000ull
#define RC_TRIG_TINY_BITS_F64     0x3e40000000000000ull
#define RC_TRIG_TINY_BITS_F32     0x39800000u

// 2/pi, and pi/2 split into a double and the double nearest the rest.
#define RC_TRIG_TWO_OVER_PI     0x1.45f306dc9c883p-1
#define RC_TRIG_HALF_PI_HI      0x1.921fb54442d18p+0
#define RC_TRIG_HALF_PI_LO      0x1.1a62633145c07p-54

// A double-double: the value hi + lo, where lo is far smaller than hi.
typedef struct rc_trig_dd {
    double hi;
    double lo;
} rc_trig_dd;

// x = k pi/2 + r, give or take whole turns: r to double-double precision and k mod 2^32.
typedef struct rc_trig_reduced {
    rc_trig_dd r;
    uint32_t quadrant;
} rc_trig_reduced;

// The same for a float, where one double holds r closely enough.
typedef struct rc_trig_reduced_f32 {
    double r;
    uint32_t quadrant;
} rc_trig_reduced_f32;

// A finite x of at least 2^20, reduced by the bits of 2/pi.
rc_trig_reduced rc_trig_reduce_huge(double x);

// 2/pi in 64-bit windows 4 bits apart, for reducing huge floats (rc_trig_reduce_huge_f32): window
// i holds the bits worth 2^(7 - 4i) down to 2^(-56 - 4i), so window 0 starts with the 8 zero bits
// before 2/pi's first, worth 1/2.
#define RC_TRIG_TWO_OVER_PI_F32_COUNT 44
extern const uint64_t rc_trig_two_over_pi_f32[RC_TRIG_TWO_OVER_PI_F32_COUNT];

// sin(x + offset pi/2) for x, not negative, that the short reduction does not take: at least 2^20,
// infinity or NaN. Out of line in trig_slow.c, the slow paths.
double rc_trig_sin_slow(double x, uint32_t offset);

// tan x for the same x.
double rc_trig_tan_slow(double x);

// ---- sums and products with their errors ----

// a + b exactly, as the rounded sum and its error, given |a| >= |b| (or a zero).
static inline rc_trig_dd rc_trig_fast_two_sum(double a, double b)
{
    double sum = a + b;

    return (rc_trig_dd) {.hi = sum, .lo = b - (sum - a)};
}


// a cut into two halves of 26 bits or fewer, whose products with each other are exact (Veltkamp).
static inline rc_trig_dd rc_trig_split(double a)
{
    double scaled = 0x1.0000002p27 * a;
    double hi = scaled - (scaled - a);

    return (rc_trig_dd) {.hi = hi, .lo = a - hi};
}


// a b exactly, as the rounded product and its error (Dekker), for products well inside the range.
static inline rc_trig_dd rc_trig_two_prod(double a, double b)
{
    rc_trig_dd a_halves = rc_trig_split(a);
    rc_trig_dd b_halves = rc_trig_split(b);
    double product = a * b;
    double high_error = a_halves.hi * b_halves.hi - product;
    double cross = a_halves.hi * b_halves.lo + a_halves.lo * b_halves.hi;

    return (rc_trig_dd) {.hi = product, .lo = (high_error + cross) + a_halves.lo * b_halves.lo};
}


// a cut to its top 26 significant bits, so the product of two such is exact.
static inline double rc_trig_top_bits(double a)
{
    return rc_bitcast_u64(rc_bitcast_f64(a) & 0xfffffffff8000000ull);
}


// a^2 as the rounded square and its error, the error to within 2^-75 of a^2, which is all the
// kernels need. a = hi + lo with hi its top 26 bits, so hi^2 is exact and so is its difference
// from the rounded square; the rest, 2 hi lo + lo^2 = (hi + a) lo, under 2^-24 of a^2, rounds
// twice. Cutting the bits takes one step where a split by Veltkamp takes three.
static inline rc_trig_dd rc_trig_square(double a)
{
    double hi = rc_trig_top_bits(a);
    double square = a * a;

    return (rc_trig_dd) {.hi = square, .lo = (hi * hi - square) + (hi + a) * (a - hi)};
}

// ---- picking without branches ----

// All ones if the quadrant is odd, else 0: a mask for rc_trig_pick.
static inline uint64_t rc_trig_odd_mask(uint32_t quadrant)
{
    return 0 - (uint64_t)(quadrant & 1u);
}


// a where the mask is all ones and b where it is 0. Done on the bits, so the compiler can't turn
// it into a branch, which random quadrants would mispredict half the time.
static inline double rc_trig_pick(uint64_t mask, double a, double b)
{
    return rc_bitcast_u64((rc_bitcast_f64(a) & mask) | (rc_bitcast_f64(b) & ~mask));
}

// ---- which path ----

// Whether |x| is below limit, given as a bit pattern. NaN never is.
static inline bool rc_trig_is_below(double x, uint64_t limit)
{
    return (rc_bitcast_f64(x) & 0x7fffffffffffffffull) < limit;
}


// The same for a float.
static inline bool rc_trig_is_below_f32(float x, uint32_t limit)
{
    return (rc_bitcast_f32(x) & 0x7fffffffu) < limit;
}

// ---- reduction ----

// k, the nearest whole number to x 2/pi for |x| below 2^50, in the low bits of the result: adding
// 1.5 2^52 leaves no room for a fraction, so the addition rounds it off. A negative k is there in
// two's complement, as it borrows from the 2^51 bit.
static inline double rc_trig_quadrant_shifted(double x)
{
    return x * RC_TRIG_TWO_OVER_PI + 0x1.8p52;
}


// Cody-Waite for |x| below 2^20. pi/2 = P1 + P2 + P3 + P4, where P1 and P2 end at 2^-53 and P3 at
// 2^-86, so with k below 2^20 every product but k P4 is exact, as is x - k P1 - k P2 (its bits all
// sit between 2^0 and 2^-53). Taking k P3 off that is exact too as a fast two-sum: either the
// difference is the larger, or both are under 2^-34 and their sum fits 53 bits. The error, under
// k 2^-140, is at most 2^-68 of the smallest r in range, and r.lo stays far below r.hi. Every step
// is odd in x, so -x gives -r exactly.
static inline rc_trig_reduced rc_trig_reduce_short(double x)
{
    double shifted = rc_trig_quadrant_shifted(x);
    double k = shifted - 0x1.8p52;
    double exact = (x - k * 0x1.921fb544p+0) - k * 0x1.0b462p-34;
    rc_trig_dd r = rc_trig_fast_two_sum(exact, k * 0x1.cb3b399ep-55);

    return (rc_trig_reduced) {
        .r = {.hi = r.hi, .lo = r.lo - k * 0x1.1701b839a2520p-88},
        .quadrant = (uint32_t)rc_bitcast_f64(shifted)
    };
}


// Cody-Waite for a float below 2^20 in magnitude, in double. pi/2 = F1 + F2 with F1 ending at
// 2^-32, so k F1 is exact for k under 2^20, and so is x - k F1; only k F2 and the subtraction
// round. With F2's own error, that is under 2^-67, against an r of at least 2^-27.8.
static inline rc_trig_reduced_f32 rc_trig_reduce_short_f32(double x)
{
    double shifted = rc_trig_quadrant_shifted(x);
    double k = shifted - 0x1.8p52;

    return (rc_trig_reduced_f32) {
        .r = (x - k * 0x1.921fb544p+0) - k * 0x1.0b4611a626331p-34,
        .quadrant = (uint32_t)rc_bitcast_f64(shifted)
    };
}


// Payne-Hanek for a finite float x of at least 2^20. With x = m 2^e, e is taken down to a multiple
// of 4 and the 24-bit m shifted up to match, to under 2^27, so a window of 2/pi from the table
// fits: it starts at the bit worth 2^(1 - e), as those before only add multiples of 4. m times its
// 101 bits is x 2/pi 2^99, give or take those multiples, made of its first 64 bits, a product kept
// mod 2^64, and the next 37, a product under 2^64. The top 64 bits of the sum hold the quadrant and
// 62 bits of fraction, rounded to the nearest quadrant: adding a half quadrant carries into the
// quadrant bits, and taking it off again leaves the fraction from -1/2 to 1/2. The low 37 bits hold
// 37 more bits of fraction. What the window leaves out is under 2^-72 of a quadrant, and as no
// float comes within 2^-29.2 of a multiple of pi/2, r is good to a relative 2^-42.
static inline rc_trig_reduced_f32 rc_trig_reduce_huge_f32(float x)
{
    uint32_t bits = rc_bitcast_f32(x);
    uint32_t index = (bits >> 25) - 36;
    uint64_t mantissa = (uint64_t)((bits & 0x7fffffu) | 0x800000u) << ((bits >> 23) & 3u);
    uint64_t low = mantissa * (rc_trig_two_over_pi_f32[index + 16] >> 27);
    uint64_t rounded = mantissa * rc_trig_two_over_pi_f32[index] + (low >> 37) + 0x2000000000000000ull;
    int64_t lead = (int64_t)(rounded & 0x3fffffffffffffffull) - 0x2000000000000000ll;
    double r = (double)lead * 0x1.921fb54442d18p-62 + (double)(low & 0x1fffffffffull) * 0x1.921fb54442d18p-99;

    RC_ASSERT(bits >= 0x49800000u && bits < 0x7f800000u);
    return (rc_trig_reduced_f32) {.r = r, .quadrant = (uint32_t)(rounded >> 62)};
}

// ---- float kernels, in double ----

// sin(r + q pi/2) is base (1 + z P(z)), z = r^2, where base is r, 1, -r or -1 by q: r's factor
// times r plus the offset. P is sine's polynomial for an even q and cosine's for an odd one. The
// zeros added are -0, so sin(-0) stays -0.
typedef struct rc_trig_kernel_f32 {
    double factor[4];
    double offset[4];
    double poly[4][4];
} rc_trig_kernel_f32;

// sin r = r (1 + z P) within a relative 2^-37.5, cos r = 1 + z P within 2^-33.9.
static const rc_trig_kernel_f32 rc_trig_sin_f32 = {
    .factor = {1.0, 0.0, -1.0, 0.0},
    .offset = {-0.0, 1.0, -0.0, -1.0},
    .poly = {
        {-0x1.5555554c71c66p-3, -0x1.ffffffcb82a77p-2, -0x1.5555554c71c66p-3, -0x1.ffffffcb82a77p-2},
        {0x1.1111086a596c2p-7, 0x1.55553c78820f6p-5, 0x1.1111086a596c2p-7, 0x1.55553c78820f6p-5},
        {-0x1.a00f7f224d58bp-13, -0x1.6c07f1600aa5fp-10, -0x1.a00f7f224d58bp-13, -0x1.6c07f1600aa5fp-10},
        {0x1.6cd1f12302d85p-19, 0x1.99169d9cc3e42p-16, 0x1.6cd1f12302d85p-19, 0x1.99169d9cc3e42p-16}
    }
};

// tan r = r N(z) / D(z), with N and D quadratics in z, within a relative 2^-35.2. Each of the
// numerator and the denominator is a base times (1 + z P(z)) as above, indexed by q's parity:
// N / D for an even q, -D / N for an odd one.
typedef struct rc_trig_tan_kernel_f32 {
    double factor[2][2];
    double offset[2][2];
    double poly[2][2][2];
} rc_trig_tan_kernel_f32;

// Numerator first, then denominator; the polynomials are N's 1 + z (a1 + z a2) and D's likewise.
static const rc_trig_tan_kernel_f32 rc_trig_tan_f32 = {
    .factor = {{1.0, 0.0}, {0.0, 1.0}},
    .offset = {{-0.0, -1.0}, {1.0, -0.0}},
    .poly = {
        {{-0x1.c81c631678d4bp-4, -0x1.c75c6e067e791p-2}, {-0x1.c75c6e067e791p-2, -0x1.c81c631678d4bp-4}},
        {{0x1.19bb5991e1970p-10, 0x1.05aaba88ef7e2p-6}, {0x1.05aaba88ef7e2p-6, 0x1.19bb5991e1970p-10}}
    }
};


// sin(r + q pi/2) for |r| <= pi/4, in double, within a relative 2^-33.9.
static inline double rc_trig_sin_quadrant_f32(double r, uint32_t quadrant)
{
    uint32_t q = quadrant & 3u;
    const rc_trig_kernel_f32 *k = &rc_trig_sin_f32;
    double z = r * r;
    double base = r * k->factor[q] + k->offset[q];
    double poly = (k->poly[0][q] + z * k->poly[1][q]) + (z * z) * (k->poly[2][q] + z * k->poly[3][q]);

    return base * (1.0 + z * poly);
}


// One side of tan's quotient: 0 for the numerator, 1 for the denominator.
static inline double rc_trig_tan_side_f32(double r, double z, uint32_t odd, uint32_t side)
{
    const rc_trig_tan_kernel_f32 *k = &rc_trig_tan_f32;
    double base = r * k->factor[side][odd] + k->offset[side][odd];

    return base * (1.0 + z * (k->poly[0][side][odd] + z * k->poly[1][side][odd]));
}


// tan(r + q pi/2) for |r| <= pi/4, in double, within a relative 2^-35.
static inline double rc_trig_tan_quadrant_f32(double r, uint32_t quadrant)
{
    uint32_t odd = quadrant & 1u;
    double z = r * r;

    return rc_trig_tan_side_f32(r, z, odd, 0) / rc_trig_tan_side_f32(r, z, odd, 1);
}

// ---- double kernels ----

// sin r = r + S r^3 + r^5 Ps(z) and cos r = 1 - z/2 + z^2 Pc(z), z = r^2, with S the double
// nearest -1/6. By quadrant both read as a + z lead + z^2 a P(z), where a is r's factor times r
// plus the constant (r, 1, -r or -1) and lead is r times sine_lead plus cosine_lead (S r, -1/2,
// -S r or 1/2). P is Ps for an even q and Pc for an odd one. The zeros added are -0, which a known
// q folds away.
typedef struct rc_trig_kernel {
    double sine[4];
    double cosine[4];
    double sine_lead[4];
    double cosine_lead[4];
    double poly[6][2];
} rc_trig_kernel;

// Ps is within a relative 2^-63 of the rest of sine, Pc within 2^-64 of the rest of cosine.
static const rc_trig_kernel rc_trig_sin = {
    .sine = {1.0, 0.0, -1.0, 0.0},
    .cosine = {-0.0, 1.0, -0.0, -1.0},
    .sine_lead = {-0x1.5555555555555p-3, 0.0, 0x1.5555555555555p-3, 0.0},
    .cosine_lead = {-0.0, -0.5, -0.0, 0.5},
    .poly = {
        {0x1.1111111111068p-7, 0x1.555555555554bp-5},
        {-0x1.a01a019ffe09bp-13, -0x1.6c16c16c15018p-10},
        {0x1.71de3a332e9f8p-19, 0x1.a01a019c8f942p-16},
        {-0x1.ae642b9d8d04ep-26, -0x1.27e4f7f1ee40dp-22},
        {0x1.61093d5cbcf0dp-33, 0x1.1ee9dc126f59fp-29},
        {-0x1.9fb849d389b59p-41, -0x1.8fa6a86831e76p-37}
    }
};


// sin(r.hi + r.lo + q pi/2) for |r| <= pi/4. a + z lead is formed exactly as a double-double from
// the square of r.hi, itself a double-double (good to 2^-75): for sine, S r rounds and so does its
// product with z, two errors of 2^-53 in a term at most 0.114 of the result; for cosine, z/2 is
// exact. What is left, z^2 a P and the first-order effects of z's rounding and of r.lo, is under
// 0.003 of the result for sine and 0.016 for cosine, and taking sin r as r in r.lo's effect on
// cosine costs at most a relative 0.06 2^-53. So adding it all and rounding once lands within 0.67
// ulp for sine (near pi/4, where the result is 0.71) and 0.61 for cosine.
static inline double rc_trig_sin_quadrant(rc_trig_dd r, uint32_t quadrant)
{
    uint32_t q = quadrant & 3u;
    uint32_t odd = quadrant & 1u;
    const rc_trig_kernel *k = &rc_trig_sin;
    rc_trig_dd z = rc_trig_square(r.hi);
    double z2 = z.hi * z.hi;
    double a = r.hi * k->sine[q] + k->cosine[q];
    double lead = r.hi * k->sine_lead[q] + k->cosine_lead[q];
    rc_trig_dd head = rc_trig_fast_two_sum(a, z.hi * lead);
    double poly = (k->poly[0][odd] + z.hi * k->poly[1][odd]) + z2 * (k->poly[2][odd] + z.hi * k->poly[3][odd]) +
        (z2 * z2) * (k->poly[4][odd] + z.hi * k->poly[5][odd]);
    double slope = k->sine[q] * (1.0 - 0.5 * z.hi) - k->cosine[q] * r.hi;
    double small = (head.lo + z.lo * lead) + r.lo * slope;

    return head.hi + ((z2 * a) * poly + small);
}


// tan r = N / D with N = r + A r^3 + r^5 Pn(z) and D = 1 - z/2 + z^2 Pd(z), A the double nearest
// -1/6, within a relative 2^-62 (fit_tan.py). N and D are built as sine and cosine are above, each
// a leading double and a small tail (under 0.003 and 0.016 of it), but with D's slope in z,
// -1/2 + 2 Pd(0) z, for the effects of z's rounding and of r.lo: N to a relative 0.23 2^-53 from
// A r^3's two roundings, D to 0.03 2^-53. tan(r + q pi/2) is N / D for an even q and -D / N for an
// odd one. The quotient of the leading parts is cut to 26 bits, as is the leading part of the
// divisor, so their product is exact and the remainder of the whole numerator over the whole
// denominator comes out with only small roundings; times the reciprocal of the whole denominator
// it corrects the quotient by at most 0.02 of it, adding a relative 0.06 2^-53. So the result is
// within 0.82 ulp. Both divisions start early and run alongside the rest.
static inline double rc_trig_tan_quadrant(rc_trig_dd r, uint32_t quadrant)
{
    uint64_t odd = rc_trig_odd_mask(quadrant);
    rc_trig_dd z = rc_trig_square(r.hi);
    double z2 = z.hi * z.hi;
    double lead = r.hi * -0x1.5555555555555p-3;
    rc_trig_dd n_head = rc_trig_fast_two_sum(r.hi, z.hi * lead);
    double n_poly = 0x1.f846f2acb757bp-8 + z.hi * (-0x1.d2aa46ba0adc6p-14 + z.hi * 0x1.0b8d237d1f0bap-22);
    double n_tail = (z2 * r.hi) * n_poly + ((n_head.lo + z.lo * lead) + r.lo * (1.0 - 0.5 * z.hi));
    double half = 0.5 * z.hi;
    double d_head = 1.0 - half;
    double d_poly = 0x1.5019ef66a7fcdp-5 + z.hi * (-0x1.1d6f3bb1cf2bfp-10 + z.hi * 0x1.f31e77bd07236p-18);
    double d_tail = z2 * d_poly + (((1.0 - d_head) - half) + (z.lo + 2.0 * (r.hi * r.lo)) * (-0.5 + z.hi * 0x1.5019ef66a7fcdp-4));
    double num_head = rc_trig_pick(odd, -d_head, n_head.hi);
    double num_tail = rc_trig_pick(odd, -d_tail, n_tail);
    double den_head = rc_trig_pick(odd, n_head.hi, d_head);
    double den_tail = rc_trig_pick(odd, n_tail, d_tail);
    double reciprocal = 1.0 / (den_head + den_tail);
    double divisor = rc_trig_top_bits(den_head);
    double quotient = rc_trig_top_bits(num_head / den_head);
    double remainder = ((num_head - quotient * divisor) - quotient * (den_head - divisor)) + (num_tail - quotient * den_tail);

    return quotient + remainder * reciprocal;
}

#endif // RC_MATH_TRIG_H_
