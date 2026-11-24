// test_math_cbrt.c
//
// Tests for the cube roots of richc/math/math.h, rc_cbrt in both widths: the special values bit for
// bit, exact cubes coming out exact, oddness, a few values for sense, and golden results that pin
// the exact bits each width gives, so that any change, or any target that computes differently,
// shows up.

#include "richc/math/math.h"

#include "richc/test.h"

#define SIGN_F32 0x80000000u
#define SIGN_F64 0x8000000000000000ull

// An input and the bits of the result for it, recorded once from our own implementation.
typedef struct golden_f32 {
    float x;
    uint32_t bits;
} golden_f32;

typedef struct golden_f64 {
    double x;
    uint64_t bits;
} golden_f64;

// Golden inputs cover exact cubes, each exponent's remainder after dividing by 3, subnormals up to
// the largest, the edges of the ranges cbrt_f64 scales, and the extremes.
static const golden_f32 cbrt_golden_f32[] = {
    {0x1p0f, 0x3f800000u},
    {0x1p1f, 0x3fa14518u},
    {0x1.8p1f, 0x3fb89ba2u},
    {0x1p2f, 0x3fcb2ff5u},
    {0x1p3f, 0x40000000u},
    {0x1.4p3f, 0x4009e242u},
    {0x1.bp4f, 0x40400000u},
    {0x1.99999ap-4f, 0x3eeda63cu},
    {0x1.0624dep-10f, 0x3dcccccdu},
    {0x1p-1f, 0x3f4b2ff5u},
    {0x1.8p0f, 0x3f9285ffu},
    {0x1.81cd6cp13f, 0x41b8e576u},
    {0x1.2a05f2p33f, 0x4506a6f4u},
    {-0x1.2a05f2p33f, 0xc506a6f4u},
    {0x1.fffffep127f, 0x54cb2ff5u},
    {0x1p-126f, 0x2a800000u},
    {0x1.16c2p-133f, 0x295109c8u},
    {0x1p-149f, 0x26a14518u},
    {0x1.fffffep-1f, 0x3f800000u},
    {-0x1.333334p-2f, 0xbf2b5ff5u},
    {-0x1p6f, 0xc0800000u},
    {-0x1.79ca1p-67f, 0xb46754a6u},
    {0x1.a5ep11f, 0x41700000u},
    {0x1.dcd65p20f, 0x42fa0000u},
    {0x1.fffffcp-127f, 0x2a7fffffu},
    {-0x1.8p-140f, 0xa8389ba2u},
    {0x1p-147f, 0x27000000u},
};

static const golden_f64 cbrt_golden_f64[] = {
    {0x1p0, 0x3ff0000000000000ull},
    {0x1p1, 0x3ff428a2f98d728bull},
    {0x1.8p1, 0x3ff7137449123ef6ull},
    {0x1p2, 0x3ff965fea53d6e3dull},
    {0x1.fffffbce4217dp2, 0x3fffffffe9a1606full},
    {0x1p3, 0x4000000000000000ull},
    {0x1.4p3, 0x40013c484138704full},
    {0x1.bp4, 0x4008000000000000ull},
    {0x1.999999999999ap-4, 0x3fddb4c7760bcff3ull},
    {0x1.0624dd2f1a9fcp-10, 0x3fb999999999999aull},
    {0x1p-1, 0x3fe965fea53d6e3dull},
    {0x1.8p0, 0x3ff250bfe1b082f5ull},
    {0x1.81cd6c8b43958p13, 0x40371caec6430a07ull},
    {0x1.2a05f2p33, 0x40a0d4de8fb11dadull},
    {-0x1.2a05f2p33, 0xc0a0d4de8fb11dadull},
    {0x1.7e43c8800759cp996, 0x54b249ad2594c37dull},
    {0x1.fffffffffffffp1023, 0x554428a2f98d728bull},
    {0x1p-1022, 0x2aa428a2f98d728bull},
    {0x0.012688b70e62bp-1022, 0x2a7a9d1b0b5d7427ull},
    {0x0.0000000000001p-1022, 0x2990000000000000ull},
    {0x1.fffffffffffffp-1, 0x3ff0000000000000ull},
    {-0x1.3333333333333p-2, 0xbfe56bfea66ef78dull},
    {-0x1p6, 0xc010000000000000ull},
    {-0x1.87e92154ef7acp-665, 0xb2173bbea4b1a818ull},
    {0x1.79ca10c924223p-67, 0x3e8cea94cb5c6ff1ull},
    {0x1p51, 0x4100000000000000ull},
    {0x1.dcd65p20, 0x405f400000000000ull},
    {0x1p600, 0x4c70000000000000ull},
    {0x1.0000000000001p600, 0x4c70000000000000ull},
    {0x1p-600, 0x3370000000000000ull},
    {0x1.fffffffffffffp-601, 0x3370000000000000ull},
    {-0x1.8p700, 0xce87137449123ef6ull},
    {0x0.fffffffffffffp-1022, 0x2aa428a2f98d728aull},
    {-0x0.0000000000018p-1022, 0xa9a7137449123ef6ull},
    {0x0.0000000004p-1022, 0x29d965fea53d6e3dull},
};


// True if x is a NaN, of any payload or sign.
static bool is_nan_f32(float x)
{
    return !rc_is_finite_f32(x) && x != x;
}


static bool is_nan_f64(double x)
{
    return !rc_is_finite_f64(x) && x != x;
}


RC_TEST(math_cbrt, special_values)
{
    float infinity_f32 = rc_bitcast_u32(0x7f800000u);
    double infinity_f64 = rc_bitcast_u64(0x7ff0000000000000ull);

    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(0.0f)), ==, 0u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(-0.0f)), ==, SIGN_F32);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(infinity_f32)), ==, 0x7f800000u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(-infinity_f32)), ==, 0xff800000u);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(0.0)), ==, 0u);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(-0.0)), ==, SIGN_F64);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(infinity_f64)), ==, 0x7ff0000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(-infinity_f64)), ==, 0xfff0000000000000ull);
    RC_CHECK_TRUE(is_nan_f32(rc_cbrt_f32(rc_bitcast_u32(0x7fc00000u))));
    RC_CHECK_TRUE(is_nan_f32(rc_cbrt_f32(rc_bitcast_u32(0xff800001u))));
    RC_CHECK_TRUE(is_nan_f64(rc_cbrt_f64(rc_bitcast_u64(0x7ff8000000000000ull))));
    RC_CHECK_TRUE(is_nan_f64(rc_cbrt_f64(rc_bitcast_u64(0xfff0000000000001ull))));
}


RC_TEST(math_cbrt, exact_cubes_f32)
{
    // Every root of up to 8 bits, whose cube fits a float, scaled by each power of 2 whose cube
    // stays in range, subnormals included
    for (int32_t root = 1; root < 256; root++) {
        for (int32_t power = -49; power <= 34; power++) {
            float scale = rc_bitcast_u32((uint32_t)(127 + power) << 23);
            float y = (float)root * scale;
            float cube = y * y * y;

            RC_CHECK(rc_cbrt_f32(cube), ==, y);
            RC_CHECK(rc_cbrt_f32(-cube), ==, -y);
        }
    }
}


RC_TEST(math_cbrt, exact_cubes_f64)
{
    // Roots of up to 17 bits, whose cubes fit a double, scaled from the subnormals to the top
    for (int64_t root = 1; root < 131072; root += 1 + root / 8) {
        for (int32_t power = -358; power <= 324; power += 7) {
            double scale = rc_bitcast_u64((uint64_t)(1023 + power) << 52);
            double y = (double)root * scale;
            double cube = y * y * y;

            RC_CHECK(rc_cbrt_f64(cube), ==, y);
            RC_CHECK(rc_cbrt_f64(-cube), ==, -y);
        }
    }

    // The largest 17-bit root, and the smallest subnormal's root
    RC_CHECK(rc_cbrt_f64(2251748274470911.0 * 0x1p-30), ==, 131071.0 * 0x1p-10);
    RC_CHECK(rc_cbrt_f64(0x1p-1074), ==, 0x1p-358);
}


RC_TEST(math_cbrt, odd)
{
    // Each finite exponent with a few mantissas
    for (uint32_t exponent = 0; exponent < 255; exponent++) {
        for (uint32_t mantissa = 0; mantissa < 0x800000u; mantissa += 0x1fffffu) {
            float x = rc_bitcast_u32((exponent << 23) | mantissa);

            RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(-x)), ==, rc_bitcast_f32(rc_cbrt_f32(x)) ^ SIGN_F32);
        }
    }

    for (uint64_t exponent = 0; exponent < 2047; exponent++) {
        for (uint64_t mantissa = 0; mantissa < 0x10000000000000ull; mantissa += 0x3ffffffffffffull) {
            double x = rc_bitcast_u64((exponent << 52) | mantissa);

            RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(-x)), ==, rc_bitcast_f64(rc_cbrt_f64(x)) ^ SIGN_F64);
        }
    }
}


RC_TEST(math_cbrt, values)
{
    RC_CHECK(rc_cbrt_f32(27.0f), ==, 3.0f);
    RC_CHECK(rc_cbrt_f32(-8.0f), ==, -2.0f);
    RC_CHECK(rc_cbrt_f32(0.125f), ==, 0.5f);
    RC_CHECK(rc_cbrt_f32(2.0f), ~=, 1.2599211f);
    RC_CHECK(rc_cbrt_f32(-100.0f), ~=, -4.6415888f);
    RC_CHECK(rc_cbrt_f64(1000.0), ==, 10.0);
    RC_CHECK(rc_cbrt_f64(-0.001), ~=, -0.1);
    RC_CHECK(rc_cbrt_f64(3.0), ~=, 1.4422495703074083);
    RC_CHECK(rc_cbrt_f64(1e300) / 1e100, ~=, 1.0);

    // cbrt(2) is the double nearest 2^(1/3)
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(2.0)), ==, 0x3ff428a2f98d728bull);
}


RC_TEST(math_cbrt, golden)
{
    for (uint32_t i = 0; i < RC_COUNT(cbrt_golden_f32); i++) {
        RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(cbrt_golden_f32[i].x)), ==, cbrt_golden_f32[i].bits);
    }

    for (uint32_t i = 0; i < RC_COUNT(cbrt_golden_f64); i++) {
        RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(cbrt_golden_f64[i].x)), ==, cbrt_golden_f64[i].bits);
    }
}
