// math/math.h
//
// The scalar maths: integer and float operations, and counting and casting bits. Where the
// hardware has an instruction for one, it compiles to that and nothing else: bsr, bsf, popcnt,
// sqrtss and roundss on x86-64-v2, and i32.clz, i32.popcnt, f32.sqrt and f32.floor on wasm. IEEE
// 754 makes the float ones exact or correctly rounded, so every target gives the same bits. MSVC
// has no __builtin_*, so it uses the SSE4.1 intrinsics and does abs and copysign on the bits; the
// overflow checks divide rather than use builtins, so it shares those.
//
// Every function carries a type suffix, which also keeps them clear of the min and max macros that
// windows.h defines. The unsuffixed name picks one by its first argument's type, so a type it
// doesn't take is a compile error.

#ifndef RC_MATH_MATH_H_
#define RC_MATH_MATH_H_

#include "richc/macros.h"

// ---- picking by type ----

#define rc_min(a, b) _Generic((a), int32_t: rc_min_i32, int64_t: rc_min_i64, float: rc_min_f32, double: rc_min_f64)(a, b)
#define rc_max(a, b) _Generic((a), int32_t: rc_max_i32, int64_t: rc_max_i64, float: rc_max_f32, double: rc_max_f64)(a, b)
#define rc_sgn(a)    _Generic((a), int32_t: rc_sgn_i32, int64_t: rc_sgn_i64)(a)
#define rc_gcd(a, b) _Generic((a), int32_t: rc_gcd_i32, int64_t: rc_gcd_i64)(a, b)

#define rc_add_overflows(a, b) _Generic((a), int64_t: rc_add_overflows_i64, uint64_t: rc_add_overflows_u64)(a, b)
#define rc_sub_overflows(a, b) _Generic((a), int64_t: rc_sub_overflows_i64, uint64_t: rc_sub_overflows_u64)(a, b)
#define rc_mul_overflows(a, b) _Generic((a), int64_t: rc_mul_overflows_i64, uint64_t: rc_mul_overflows_u64)(a, b)

#define rc_deg_to_rad(degrees) _Generic((degrees), float: rc_deg_to_rad_f32, double: rc_deg_to_rad_f64)(degrees)
#define rc_sqrt(x)             _Generic((x), float: rc_sqrt_f32, double: rc_sqrt_f64)(x)
#define rc_abs(x)              _Generic((x), float: rc_abs_f32, double: rc_abs_f64)(x)
#define rc_copysign(x, y)      _Generic((x), float: rc_copysign_f32, double: rc_copysign_f64)(x, y)
#define rc_floor(x)            _Generic((x), float: rc_floor_f32, double: rc_floor_f64)(x)
#define rc_ceil(x)             _Generic((x), float: rc_ceil_f32, double: rc_ceil_f64)(x)
#define rc_trunc(x)            _Generic((x), float: rc_trunc_f32, double: rc_trunc_f64)(x)
#define rc_round_even(x)       _Generic((x), float: rc_round_even_f32, double: rc_round_even_f64)(x)
#define rc_is_finite(x)        _Generic((x), float: rc_is_finite_f32, double: rc_is_finite_f64)(x)

#define rc_sin(x)      _Generic((x), float: rc_sin_f32, double: rc_sin_f64)(x)
#define rc_cos(x)      _Generic((x), float: rc_cos_f32, double: rc_cos_f64)(x)
#define rc_tan(x)      _Generic((x), float: rc_tan_f32, double: rc_tan_f64)(x)
#define rc_asin(x)     _Generic((x), float: rc_asin_f32, double: rc_asin_f64)(x)
#define rc_acos(x)     _Generic((x), float: rc_acos_f32, double: rc_acos_f64)(x)
#define rc_atan(x)     _Generic((x), float: rc_atan_f32, double: rc_atan_f64)(x)
#define rc_atan2(y, x) _Generic((y), float: rc_atan2_f32, double: rc_atan2_f64)(y, x)
#define rc_exp(x)      _Generic((x), float: rc_exp_f32, double: rc_exp_f64)(x)
#define rc_log(x)      _Generic((x), float: rc_log_f32, double: rc_log_f64)(x)
#define rc_log2(x)     _Generic((x), float: rc_log2_f32, double: rc_log2_f64)(x)
#define rc_pow(x, y)   _Generic((x), float: rc_pow_f32, double: rc_pow_f64)(x, y)
#define rc_cbrt(x)     _Generic((x), float: rc_cbrt_f32, double: rc_cbrt_f64)(x)

#define rc_clz(a)      _Generic((a), uint32_t: rc_clz_u32, uint64_t: rc_clz_u64)(a)
#define rc_ctz(a)      _Generic((a), uint32_t: rc_ctz_u32, uint64_t: rc_ctz_u64)(a)
#define rc_popcount(a) _Generic((a), uint32_t: rc_popcount_u32, uint64_t: rc_popcount_u64)(a)
#define rc_bitcast(x)  _Generic((x), float: rc_bitcast_f32, double: rc_bitcast_f64, uint32_t: rc_bitcast_u32, uint64_t: rc_bitcast_u64)(x)

// ---- bit patterns ----

// The bits of a float, as a uint32_t.
static inline uint32_t rc_bitcast_f32(float x)
{
    union {
        float f;
        uint32_t u;
    } pun = {.f = x};

    return pun.u;
}

// The bits of a double, as a uint64_t.
static inline uint64_t rc_bitcast_f64(double x)
{
    union {
        double f;
        uint64_t u;
    } pun = {.f = x};

    return pun.u;
}

// The float with the bits of x.
static inline float rc_bitcast_u32(uint32_t x)
{
    union {
        uint32_t u;
        float f;
    } pun = {.u = x};

    return pun.f;
}

// The double with the bits of x.
static inline double rc_bitcast_u64(uint64_t x)
{
    union {
        uint64_t u;
        double f;
    } pun = {.u = x};

    return pun.f;
}

// ---- counting bits ----

// The number of zero bits above the highest set bit; 32 for 0.
static inline uint32_t rc_clz_u32(uint32_t a)
{
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index = 0;

    return _BitScanReverse(&index, a) ? 31u - (uint32_t)index : 32u;
#else
    return a == 0 ? 32u : (uint32_t)__builtin_clz(a);
#endif
}

// The number of zero bits above the highest set bit; 64 for 0.
static inline uint32_t rc_clz_u64(uint64_t a)
{
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index = 0;

    return _BitScanReverse64(&index, a) ? 63u - (uint32_t)index : 64u;
#else
    return a == 0 ? 64u : (uint32_t)__builtin_clzll(a);
#endif
}

// The number of zero bits below the lowest set bit; 32 for 0.
static inline uint32_t rc_ctz_u32(uint32_t a)
{
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index = 0;

    return _BitScanForward(&index, a) ? (uint32_t)index : 32u;
#else
    return a == 0 ? 32u : (uint32_t)__builtin_ctz(a);
#endif
}

// The number of zero bits below the lowest set bit; 64 for 0.
static inline uint32_t rc_ctz_u64(uint64_t a)
{
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index = 0;

    return _BitScanForward64(&index, a) ? (uint32_t)index : 64u;
#else
    return a == 0 ? 64u : (uint32_t)__builtin_ctzll(a);
#endif
}

// The number of set bits.
static inline uint32_t rc_popcount_u32(uint32_t a)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return (uint32_t)__popcnt(a);
#else
    return (uint32_t)__builtin_popcount(a);
#endif
}

// The number of set bits.
static inline uint32_t rc_popcount_u64(uint64_t a)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return (uint32_t)__popcnt64(a);
#else
    return (uint32_t)__builtin_popcountll(a);
#endif
}

// ---- smaller and larger ----

// The smaller of a and b.
static inline int32_t rc_min_i32(int32_t a, int32_t b)
{
    return a < b ? a : b;
}

// The smaller of a and b.
static inline int64_t rc_min_i64(int64_t a, int64_t b)
{
    return a < b ? a : b;
}

// The smaller of a and b; b if either is NaN.
static inline float rc_min_f32(float a, float b)
{
    return a < b ? a : b;
}

// The smaller of a and b; b if either is NaN.
static inline double rc_min_f64(double a, double b)
{
    return a < b ? a : b;
}

// The larger of a and b.
static inline int32_t rc_max_i32(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

// The larger of a and b.
static inline int64_t rc_max_i64(int64_t a, int64_t b)
{
    return a > b ? a : b;
}

// The larger of a and b; b if either is NaN.
static inline float rc_max_f32(float a, float b)
{
    return a > b ? a : b;
}

// The larger of a and b; b if either is NaN.
static inline double rc_max_f64(double a, double b)
{
    return a > b ? a : b;
}

// ---- sign and divisors ----

// -1, 0 or 1, as a is negative, zero or positive.
static inline int32_t rc_sgn_i32(int32_t a)
{
    return (a > 0) - (a < 0);
}

// -1, 0 or 1, as a is negative, zero or positive.
static inline int64_t rc_sgn_i64(int64_t a)
{
    return (a > 0) - (a < 0);
}

// The greatest common divisor, never negative; 0 when both are 0. The answer must fit, so a and b
// can't be INT32_MIN and 0, or INT32_MIN twice.
static inline int32_t rc_gcd_i32(int32_t a, int32_t b)
{
    // Magnitudes, in unsigned so that INT32_MIN's fits and nothing divides INT32_MIN by -1
    uint32_t x = a < 0 ? 0u - (uint32_t)a : (uint32_t)a;
    uint32_t y = b < 0 ? 0u - (uint32_t)b : (uint32_t)b;

    while (y != 0) {
        uint32_t remainder = x % y;

        x = y;
        y = remainder;
    }

    RC_ASSERT(x <= INT32_MAX);
    return (int32_t)x;
}

// The greatest common divisor, never negative; 0 when both are 0. The answer must fit, so a and b
// can't be INT64_MIN and 0, or INT64_MIN twice.
static inline int64_t rc_gcd_i64(int64_t a, int64_t b)
{
    // Magnitudes, in unsigned so that INT64_MIN's fits and nothing divides INT64_MIN by -1
    uint64_t x = a < 0 ? 0u - (uint64_t)a : (uint64_t)a;
    uint64_t y = b < 0 ? 0u - (uint64_t)b : (uint64_t)b;

    while (y != 0) {
        uint64_t remainder = x % y;

        x = y;
        y = remainder;
    }

    RC_ASSERT(x <= INT64_MAX);
    return (int64_t)x;
}

// ---- overflow ----

// True if a + b doesn't fit in a uint64_t.
static inline bool rc_add_overflows_u64(uint64_t a, uint64_t b)
{
    return b > UINT64_MAX - a;
}

// True if a - b doesn't fit in a uint64_t: if it would be negative.
static inline bool rc_sub_overflows_u64(uint64_t a, uint64_t b)
{
    return b > a;
}

// True if a * b doesn't fit in a uint64_t.
static inline bool rc_mul_overflows_u64(uint64_t a, uint64_t b)
{
    return a != 0 && b > UINT64_MAX / a;
}

// True if a + b doesn't fit in an int64_t.
static inline bool rc_add_overflows_i64(int64_t a, int64_t b)
{
    return b > 0 ? a > INT64_MAX - b : a < INT64_MIN - b;
}

// True if a - b doesn't fit in an int64_t.
static inline bool rc_sub_overflows_i64(int64_t a, int64_t b)
{
    return b < 0 ? a > INT64_MAX + b : a < INT64_MIN + b;
}

// True if a * b doesn't fit in an int64_t. Each case divides the limit the product must stay
// within by one operand, which can't itself overflow.
static inline bool rc_mul_overflows_i64(int64_t a, int64_t b)
{
    if (a == 0 || b == 0) {
        return false;
    }

    if (a > 0) {
        return b > 0 ? a > INT64_MAX / b : b < INT64_MIN / a;
    }

    return b > 0 ? a < INT64_MIN / b : a < INT64_MAX / b;
}

// ---- angles ----

// An angle in degrees, in radians.
static inline float rc_deg_to_rad_f32(float degrees)
{
    return degrees * 0.0174532925f;
}

// An angle in degrees, in radians.
static inline double rc_deg_to_rad_f64(double degrees)
{
    return degrees * 0.017453292519943295;
}

// ---- MSVC's rounding ----

// x rounded by an SSE4.1 rounding mode, which must be a constant.
#if defined(_MSC_VER) && !defined(__clang__)
#  define RC_ROUND_F32_(x, mode) _mm_cvtss_f32(_mm_round_ss(_mm_set_ss(x), _mm_set_ss(x), (mode) | _MM_FROUND_NO_EXC))
#  define RC_ROUND_F64_(x, mode) _mm_cvtsd_f64(_mm_round_sd(_mm_set_sd(x), _mm_set_sd(x), (mode) | _MM_FROUND_NO_EXC))
#endif

// ---- square root ----

// The square root of x; NaN if x is negative.
static inline float rc_sqrt_f32(float x)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(x)));
#else
    return __builtin_sqrtf(x);
#endif
}

// The square root of x; NaN if x is negative.
static inline double rc_sqrt_f64(double x)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return _mm_cvtsd_f64(_mm_sqrt_sd(_mm_set_sd(x), _mm_set_sd(x)));
#else
    return __builtin_sqrt(x);
#endif
}

// ---- float sign ----

// x without its sign.
static inline float rc_abs_f32(float x)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return rc_bitcast_u32(rc_bitcast_f32(x) & 0x7fffffffu);
#else
    return __builtin_fabsf(x);
#endif
}

// x without its sign.
static inline double rc_abs_f64(double x)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return rc_bitcast_u64(rc_bitcast_f64(x) & 0x7fffffffffffffffull);
#else
    return __builtin_fabs(x);
#endif
}

// x with the sign of y.
static inline float rc_copysign_f32(float x, float y)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return rc_bitcast_u32((rc_bitcast_f32(x) & 0x7fffffffu) | (rc_bitcast_f32(y) & 0x80000000u));
#else
    return __builtin_copysignf(x, y);
#endif
}

// x with the sign of y.
static inline double rc_copysign_f64(double x, double y)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return rc_bitcast_u64((rc_bitcast_f64(x) & 0x7fffffffffffffffull) | (rc_bitcast_f64(y) & 0x8000000000000000ull));
#else
    return __builtin_copysign(x, y);
#endif
}

// ---- rounding ----

// The largest whole number not above x.
static inline float rc_floor_f32(float x)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return RC_ROUND_F32_(x, _MM_FROUND_TO_NEG_INF);
#else
    return __builtin_floorf(x);
#endif
}

// The largest whole number not above x.
static inline double rc_floor_f64(double x)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return RC_ROUND_F64_(x, _MM_FROUND_TO_NEG_INF);
#else
    return __builtin_floor(x);
#endif
}

// The smallest whole number not below x.
static inline float rc_ceil_f32(float x)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return RC_ROUND_F32_(x, _MM_FROUND_TO_POS_INF);
#else
    return __builtin_ceilf(x);
#endif
}

// The smallest whole number not below x.
static inline double rc_ceil_f64(double x)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return RC_ROUND_F64_(x, _MM_FROUND_TO_POS_INF);
#else
    return __builtin_ceil(x);
#endif
}

// x without its fraction: rounded towards zero.
static inline float rc_trunc_f32(float x)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return RC_ROUND_F32_(x, _MM_FROUND_TO_ZERO);
#else
    return __builtin_truncf(x);
#endif
}

// x without its fraction: rounded towards zero.
static inline double rc_trunc_f64(double x)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return RC_ROUND_F64_(x, _MM_FROUND_TO_ZERO);
#else
    return __builtin_trunc(x);
#endif
}

// The nearest whole number to x, halves going to the even one.
static inline float rc_round_even_f32(float x)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return RC_ROUND_F32_(x, _MM_FROUND_TO_NEAREST_INT);
#else
    return __builtin_roundevenf(x);
#endif
}

// The nearest whole number to x, halves going to the even one.
static inline double rc_round_even_f64(double x)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return RC_ROUND_F64_(x, _MM_FROUND_TO_NEAREST_INT);
#else
    return __builtin_roundeven(x);
#endif
}

// ---- classifying ----

// True unless x is infinite or NaN, which are the values whose exponent bits are all set.
static inline bool rc_is_finite_f32(float x)
{
    return (rc_bitcast_f32(x) & 0x7f800000u) != 0x7f800000u;
}

// True unless x is infinite or NaN, which are the values whose exponent bits are all set.
static inline bool rc_is_finite_f64(double x)
{
    return (rc_bitcast_f64(x) & 0x7ff0000000000000ull) != 0x7ff0000000000000ull;
}

// ---- transcendentals ----

// Each is our own, written from a specification with coefficients we fitted ourselves, in a file
// of its own under src/math, so a program only links the ones it calls. IEEE 754 doesn't pin these
// down, so only by computing them ourselves do we get the same bits on every target. Angles are in
// radians. The f32 ones are within 0.52 ulp of the true value (most within 0.51, all but correctly
// rounded) and the f64 ones within 0.8; test/math_accuracy.c holds each one's limit and checks it.

// The sine of x.
float rc_sin_f32(float x);

// The sine of x.
double rc_sin_f64(double x);

// The cosine of x.
float rc_cos_f32(float x);

// The cosine of x.
double rc_cos_f64(double x);

// The tangent of x.
float rc_tan_f32(float x);

// The tangent of x.
double rc_tan_f64(double x);

// The angle in [-pi/2, pi/2] whose sine is x; NaN outside [-1, 1].
float rc_asin_f32(float x);

// The angle in [-pi/2, pi/2] whose sine is x; NaN outside [-1, 1].
double rc_asin_f64(double x);

// The angle in [0, pi] whose cosine is x; NaN outside [-1, 1].
float rc_acos_f32(float x);

// The angle in [0, pi] whose cosine is x; NaN outside [-1, 1].
double rc_acos_f64(double x);

// The angle in [-pi/2, pi/2] whose tangent is x.
float rc_atan_f32(float x);

// The angle in [-pi/2, pi/2] whose tangent is x.
double rc_atan_f64(double x);

// The angle in [-pi, pi] of the point (x, y) from the x axis.
float rc_atan2_f32(float y, float x);

// The angle in [-pi, pi] of the point (x, y) from the x axis.
double rc_atan2_f64(double y, double x);

// e to the power x.
float rc_exp_f32(float x);

// e to the power x.
double rc_exp_f64(double x);

// The natural logarithm of x; -infinity for 0, NaN below it.
float rc_log_f32(float x);

// The natural logarithm of x; -infinity for 0, NaN below it.
double rc_log_f64(double x);

// The base 2 logarithm of x; -infinity for 0, NaN below it.
float rc_log2_f32(float x);

// The base 2 logarithm of x; -infinity for 0, NaN below it.
double rc_log2_f64(double x);

// x to the power y, with C's pow for the special cases.
float rc_pow_f32(float x, float y);

// x to the power y, with C's pow for the special cases.
double rc_pow_f64(double x, double y);

// The cube root of x, negative for a negative x.
float rc_cbrt_f32(float x);

// The cube root of x, negative for a negative x.
double rc_cbrt_f64(double x);

#endif // RC_MATH_MATH_H_
