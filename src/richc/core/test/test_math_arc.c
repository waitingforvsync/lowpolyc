// test_math_arc.c
//
// Tests for the inverse trigonometric functions in richc/math/math.h: asin, acos, atan and atan2.
// The expected bits are golden values, recorded from the port once it matched musl on every float
// input and on hundreds of millions of doubles, so every target must give exactly these.

#include "richc/math/math.h"

#include "richc/test.h"

// ---- asin ----

RC_TEST(math_asin, known_values)
{
    RC_CHECK(rc_asin_f32(0.0f), ==, 0.0f);
    RC_CHECK(rc_asin_f64(0.0), ==, 0.0);

    // asin(1) is pi/2, rounded to each precision
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(1.0f)), ==, 0x3fc90fdbu);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(1.0)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_asin_f32(-1.0f), ==, -rc_asin_f32(1.0f));
    RC_CHECK(rc_asin_f64(-1.0), ==, -rc_asin_f64(1.0));
}


RC_TEST(math_asin, f32)
{
    // Zeros keep their sign, and below 2^-12 asin(x) is x
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(0.0f)), ==, 0x00000000u);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(-0.0f)), ==, 0x80000000u);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(0x1p-149f)), ==, 0x00000001u);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(-0x1p-149f)), ==, 0x80000001u);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(0x1p-126f)), ==, 0x00800000u);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(0x1.fffffep-13f)), ==, 0x397fffffu);

    // The series below 0.5
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(0x1p-12f)), ==, 0x39800000u);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(0.1f)), ==, 0x3dcd2494u);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(-0.25f)), ==, 0xbe815f4eu);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(0.3f)), ==, 0x3e9c00adu);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(0x1.fffffep-2f)), ==, 0x3f060a91u);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(-0x1.fffffep-2f)), ==, 0xbf060a91u);

    // From 0.5 to 1, through the square root
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(0.5f)), ==, 0x3f060a92u);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(-0.5f)), ==, 0xbf060a92u);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(0.6f)), ==, 0x3f24bc7eu);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(0.75f)), ==, 0x3f591a99u);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(-0.9f)), ==, 0xbf8f549bu);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(0.99f)), ==, 0x3fb6f1e4u);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(0x1.fffffep-1f)), ==, 0x3fc9048au);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(-0x1.fffffep-1f)), ==, 0xbfc9048au);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(1.0f)), ==, 0x3fc90fdbu);
    RC_CHECK(rc_bitcast_f32(rc_asin_f32(-1.0f)), ==, 0xbfc90fdbu);
}


RC_TEST(math_asin, f64)
{
    // Zeros keep their sign, and below 2^-26 asin(x) is x
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(0.0)), ==, 0x0000000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(-0.0)), ==, 0x8000000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(0x1p-1074)), ==, 0x0000000000000001ull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(-0x1p-1074)), ==, 0x8000000000000001ull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(0x1p-1022)), ==, 0x0010000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(0x1.fffffffffffffp-27)), ==, 0x3e4fffffffffffffull);

    // The series below 0.5
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(0x1p-26)), ==, 0x3e50000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(0.1)), ==, 0x3fb9a49276037884ull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(-0.25)), ==, 0xbfd02be9ce0b87cdull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(0.3)), ==, 0x3fd380159e14f6ffull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(0x1.fffffffffffffp-2)), ==, 0x3fe0c152382d7365ull);

    // From 0.5 to 0.975, with the split root
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(0.5)), ==, 0x3fe0c152382d7366ull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(-0.5)), ==, 0xbfe0c152382d7366ull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(0.6)), ==, 0x3fe4978fa3269ee1ull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(-0.9)), ==, 0xbff1ea93705fa172ull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(0x1.f3332ffffffffp-1)), ==, 0x3ff58c2ae9ab49e8ull);

    // From 0.975 to 1, with the plain root
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(0x1.f3333p-1)), ==, 0x3ff58c2ae9ab49eaull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(0.99)), ==, 0x3ff6de3c6f33d51dull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(-0.999)), ==, 0xbff86ac9ad18f803ull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(0x1.fffffffffffffp-1)), ==, 0x3ff921fb50442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(-0x1.fffffffffffffp-1)), ==, 0xbff921fb50442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(1.0)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_asin_f64(-1.0)), ==, 0xbff921fb54442d18ull);
}


RC_TEST(math_asin, nan)
{
    float infinity_f32 = rc_bitcast_u32(0x7f800000u);
    float nan_f32 = rc_bitcast_u32(0x7fc00000u);
    double infinity_f64 = rc_bitcast_u64(0x7ff0000000000000ull);
    double nan_f64 = rc_bitcast_u64(0x7ff8000000000000ull);
    float above_one_f32 = rc_asin_f32(0x1.000002p0f);
    float nan_result_f32 = rc_asin_f32(nan_f32);
    double above_one_f64 = rc_asin_f64(0x1.0000000000001p0);
    double nan_result_f64 = rc_asin_f64(nan_f64);

    // Outside [-1, 1] there is no answer
    RC_CHECK_TRUE(above_one_f32 != above_one_f32);
    RC_CHECK_TRUE(nan_result_f32 != nan_result_f32);
    RC_CHECK_TRUE(above_one_f64 != above_one_f64);
    RC_CHECK_TRUE(nan_result_f64 != nan_result_f64);
    RC_CHECK_FALSE(rc_is_finite_f32(rc_asin_f32(-2.0f)));
    RC_CHECK_FALSE(rc_is_finite_f32(rc_asin_f32(infinity_f32)));
    RC_CHECK_FALSE(rc_is_finite_f32(rc_asin_f32(-infinity_f32)));
    RC_CHECK_FALSE(rc_is_finite_f64(rc_asin_f64(-2.0)));
    RC_CHECK_FALSE(rc_is_finite_f64(rc_asin_f64(infinity_f64)));
    RC_CHECK_FALSE(rc_is_finite_f64(rc_asin_f64(-infinity_f64)));
}


// ---- acos ----

RC_TEST(math_acos, known_values)
{
    RC_CHECK(rc_acos_f32(1.0f), ==, 0.0f);
    RC_CHECK(rc_acos_f64(1.0), ==, 0.0);

    // acos(0) is pi/2 and acos(-1) is pi. In float they come from the high half of pi/2, a float
    // below the nearest, as in musl
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(0.0f)), ==, 0x3fc90fdau);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(0.0)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(-1.0f)), ==, 0x40490fdau);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(-1.0)), ==, 0x400921fb54442d18ull);
}


RC_TEST(math_acos, f32)
{
    // At or below 2^-26 acos(x) is pi/2, whatever the sign
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(0.0f)), ==, 0x3fc90fdau);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(-0.0f)), ==, 0x3fc90fdau);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(0x1p-149f)), ==, 0x3fc90fdau);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(-0x1p-126f)), ==, 0x3fc90fdau);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(0x1p-26f)), ==, 0x3fc90fdau);

    // The series below 0.5
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(0x1.000002p-26f)), ==, 0x3fc90fdbu);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(-0x1.000002p-26f)), ==, 0x3fc90fdbu);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(0x1p-12f)), ==, 0x3fc907dbu);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(0.1f)), ==, 0x3fbc3d91u);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(-0.3f)), ==, 0x3ff01006u);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(0x1.fffffep-2f)), ==, 0x3f860a92u);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(-0x1.fffffep-2f)), ==, 0x40060a92u);

    // From -1 to -0.5
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(-0.5f)), ==, 0x40060a92u);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(-0.75f)), ==, 0x401ace94u);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(-0.99f)), ==, 0x404000dfu);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(-0x1.fffffep-1f)), ==, 0x40490a32u);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(-1.0f)), ==, 0x40490fdau);

    // From 0.5 to 1, with the split root
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(0.5f)), ==, 0x3f860a92u);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(0.75f)), ==, 0x3f39051du);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(0.99f)), ==, 0x3e10efb5u);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(0x1.fffffep-1f)), ==, 0x39b504f3u);
    RC_CHECK(rc_bitcast_f32(rc_acos_f32(1.0f)), ==, 0x00000000u);
}


RC_TEST(math_acos, f64)
{
    // At or below 2^-57 acos(x) is pi/2, whatever the sign
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(0.0)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(-0.0)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(0x1p-1074)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(-0x1p-1022)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(0x1.00000ffffffffp-57)), ==, 0x3ff921fb54442d18ull);

    // The series below 0.5
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(0x1.00001p-57)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(-0x1.00001p-57)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(0x1p-26)), ==, 0x3ff921fb50442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(0.1)), ==, 0x3ff787b22ce3f590ull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(-0.3)), ==, 0x3ffe0200bbc96ad8ull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(0x1.fffffffffffffp-2)), ==, 0x3ff0c152382d7366ull);

    // From -1 to -0.5
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(-0.5)), ==, 0x4000c152382d7366ull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(-0.75)), ==, 0x400359d26f93b6c3ull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(-0.999)), ==, 0x4008c66280ae928eull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(-0x1.fffffffffffffp-1)), ==, 0x400921fb52442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(-1.0)), ==, 0x400921fb54442d18ull);

    // From 0.5 to 1, with the split root
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(0.5)), ==, 0x3ff0c152382d7366ull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(0.75)), ==, 0x3fe720a392c1d955ull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(0.999)), ==, 0x3fa6e634e566a29eull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(0x1.fffffffffffffp-1)), ==, 0x3e50000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_acos_f64(1.0)), ==, 0x0000000000000000ull);
}


RC_TEST(math_acos, nan)
{
    float infinity_f32 = rc_bitcast_u32(0x7f800000u);
    float nan_f32 = rc_bitcast_u32(0x7fc00000u);
    double infinity_f64 = rc_bitcast_u64(0x7ff0000000000000ull);
    double nan_f64 = rc_bitcast_u64(0x7ff8000000000000ull);
    float below_minus_one_f32 = rc_acos_f32(-0x1.000002p0f);
    float nan_result_f32 = rc_acos_f32(nan_f32);
    double below_minus_one_f64 = rc_acos_f64(-0x1.0000000000001p0);
    double nan_result_f64 = rc_acos_f64(nan_f64);

    // Outside [-1, 1] there is no answer
    RC_CHECK_TRUE(below_minus_one_f32 != below_minus_one_f32);
    RC_CHECK_TRUE(nan_result_f32 != nan_result_f32);
    RC_CHECK_TRUE(below_minus_one_f64 != below_minus_one_f64);
    RC_CHECK_TRUE(nan_result_f64 != nan_result_f64);
    RC_CHECK_FALSE(rc_is_finite_f32(rc_acos_f32(2.0f)));
    RC_CHECK_FALSE(rc_is_finite_f32(rc_acos_f32(infinity_f32)));
    RC_CHECK_FALSE(rc_is_finite_f32(rc_acos_f32(-infinity_f32)));
    RC_CHECK_FALSE(rc_is_finite_f64(rc_acos_f64(2.0)));
    RC_CHECK_FALSE(rc_is_finite_f64(rc_acos_f64(infinity_f64)));
    RC_CHECK_FALSE(rc_is_finite_f64(rc_acos_f64(-infinity_f64)));
}


// ---- atan ----

RC_TEST(math_atan, known_values)
{
    float infinity_f32 = rc_bitcast_u32(0x7f800000u);
    double infinity_f64 = rc_bitcast_u64(0x7ff0000000000000ull);

    RC_CHECK(rc_atan_f32(0.0f), ==, 0.0f);
    RC_CHECK(rc_atan_f64(0.0), ==, 0.0);

    // atan(1) is pi/4 and atan(infinity) pi/2. The float pi/2 is the high half of the tabled one, a
    // float below the nearest, as in musl
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(1.0f)), ==, 0x3f490fdbu);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(1.0)), ==, 0x3fe921fb54442d18ull);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(infinity_f32)), ==, 0x3fc90fdau);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(infinity_f64)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_atan_f32(-1.0f), ==, -rc_atan_f32(1.0f));
    RC_CHECK(rc_atan_f64(-1.0), ==, -rc_atan_f64(1.0));
}


RC_TEST(math_atan, f32)
{
    float infinity = rc_bitcast_u32(0x7f800000u);

    // Zeros keep their sign, and below 2^-12 atan(x) is x
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(0.0f)), ==, 0x00000000u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(-0.0f)), ==, 0x80000000u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(0x1p-149f)), ==, 0x00000001u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(-0x1p-126f)), ==, 0x80800000u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(0x1.fffffep-13f)), ==, 0x397fffffu);

    // The series alone below 7/16
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(0x1p-12f)), ==, 0x39800000u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(0.1f)), ==, 0x3dcc1f14u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(-0.25f)), ==, 0xbe7adbb0u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(0x1.bffffep-2f)), ==, 0x3ed32775u);

    // Offset from atan(1/2), atan(1), atan(3/2) and atan(infinity) in turn
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(0.4375f)), ==, 0x3ed32776u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(0.5f)), ==, 0x3eed6338u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(-0.6f)), ==, 0xbf0a58efu);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(0.6875f)), ==, 0x3f1a2f81u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(1.0f)), ==, 0x3f490fdbu);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(-1.1f)), ==, 0xbf553e43u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(1.1875f)), ==, 0x3f5ef387u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(2.0f)), ==, 0x3f8db70du);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(2.4375f)), ==, 0x3f973ab9u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(-10.0f)), ==, 0xbfbc4de9u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(1000000.0f)), ==, 0x3fc90fd2u);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(0x1.fffffep25f)), ==, 0x3fc90fdbu);

    // From 2^26 up, +-pi/2
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(0x1p26f)), ==, 0x3fc90fdau);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(-0x1p26f)), ==, 0xbfc90fdau);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(0x1.fffffep127f)), ==, 0x3fc90fdau);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(infinity)), ==, 0x3fc90fdau);
    RC_CHECK(rc_bitcast_f32(rc_atan_f32(-infinity)), ==, 0xbfc90fdau);
}


RC_TEST(math_atan, f64)
{
    double infinity = rc_bitcast_u64(0x7ff0000000000000ull);

    // Zeros keep their sign, and below 2^-27 atan(x) is x
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(0.0)), ==, 0x0000000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(-0.0)), ==, 0x8000000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(0x1p-1074)), ==, 0x0000000000000001ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(-0x1p-1022)), ==, 0x8010000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(0x1.fffffffffffffp-28)), ==, 0x3e3fffffffffffffull);

    // The series alone below 7/16
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(0x1p-27)), ==, 0x3e40000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(0.1)), ==, 0x3fb983e282e2cc4dull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(-0.25)), ==, 0xbfcf5b75f92c80ddull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(0x1.bffffffffffffp-2)), ==, 0x3fda64eec3cc23fcull);

    // Offset from atan(1/2), atan(1), atan(3/2) and atan(infinity) in turn
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(0.4375)), ==, 0x3fda64eec3cc23fdull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(0.5)), ==, 0x3fddac670561bb4full);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(-0.6)), ==, 0xbfe14b1dd5f90ce1ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(0.6875)), ==, 0x3fe345f01cce37bbull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(1.0)), ==, 0x3fe921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(-1.1)), ==, 0xbfeaa7c8545183cdull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(1.1875)), ==, 0x3febde70ed439fe7ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(2.0)), ==, 0x3ff1b6e192ebbe44ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(2.4375)), ==, 0x3ff2e75728833a54ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(-10.0)), ==, 0xbff789bd2c160054ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(1e10)), ==, 0x3ff921fb543d4de0ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(0x1.fffffffffffffp65)), ==, 0x3ff921fb54442d18ull);

    // From 2^66 up, +-pi/2
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(0x1p66)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(-0x1p66)), ==, 0xbff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(0x1.fffffffffffffp1023)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(infinity)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan_f64(-infinity)), ==, 0xbff921fb54442d18ull);
}


RC_TEST(math_atan, nan)
{
    float nan_f32 = rc_atan_f32(rc_bitcast_u32(0x7fc00000u));
    double nan_f64 = rc_atan_f64(rc_bitcast_u64(0x7ff8000000000000ull));

    RC_CHECK_TRUE(nan_f32 != nan_f32);
    RC_CHECK_TRUE(nan_f64 != nan_f64);
    RC_CHECK_FALSE(rc_is_finite_f32(nan_f32));
    RC_CHECK_FALSE(rc_is_finite_f64(nan_f64));
}


// ---- atan2 ----

RC_TEST(math_atan2, known_values)
{
    // atan2(y, 1) is atan(y), and the diagonal is pi/4
    RC_CHECK(rc_atan2_f32(0.5f, 1.0f), ==, rc_atan_f32(0.5f));
    RC_CHECK(rc_atan2_f64(0.5, 1.0), ==, rc_atan_f64(0.5));
    RC_CHECK(rc_atan2_f32(1.0f, 1.0f), ==, rc_atan_f32(1.0f));
    RC_CHECK(rc_atan2_f64(3.0, 3.0), ==, rc_atan_f64(1.0));

    // Straight left is pi, straight up pi/2
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(0.0, -1.0)), ==, 0x400921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(1.0, 0.0)), ==, 0x3ff921fb54442d18ull);
}


RC_TEST(math_atan2, f32)
{
    float infinity = rc_bitcast_u32(0x7f800000u);

    // One in each quadrant, and x = 1 as plain atan
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(3.0f, 4.0f)), ==, 0x3f24bc7du);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(3.0f, -4.0f)), ==, 0x401fe0bcu);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-3.0f, -4.0f)), ==, 0xc01fe0bcu);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-3.0f, 4.0f)), ==, 0xbf24bc7du);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-0.7f, 1.0f)), ==, 0xbf1c5889u);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(1e-3f, -2.5f)), ==, 0x4049094du);

    // Both zeros against both zeros, and zero y against finite x
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(0.0f, 0.0f)), ==, 0x00000000u);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-0.0f, 0.0f)), ==, 0x80000000u);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(0.0f, -0.0f)), ==, 0x40490fdbu);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-0.0f, -0.0f)), ==, 0xc0490fdbu);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(0.0f, 5.0f)), ==, 0x00000000u);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-0.0f, -5.0f)), ==, 0xc0490fdbu);

    // Zero x, +-pi/2
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(3.0f, 0.0f)), ==, 0x3fc90fdbu);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-3.0f, -0.0f)), ==, 0xbfc90fdbu);

    // Infinities against infinities, and against finite values
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(infinity, infinity)), ==, 0x3f490fdbu);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-infinity, infinity)), ==, 0xbf490fdbu);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(infinity, -infinity)), ==, 0x4016cbe4u);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-infinity, -infinity)), ==, 0xc016cbe4u);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(1.0f, infinity)), ==, 0x00000000u);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-1.0f, infinity)), ==, 0x80000000u);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(1.0f, -infinity)), ==, 0x40490fdbu);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-1.0f, -infinity)), ==, 0xc0490fdbu);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(infinity, 1.0f)), ==, 0x3fc90fdau);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-infinity, -1.0f)), ==, 0xbfc90fdbu);

    // Steep, beyond 2^26, and shallow, below 2^-26, on each side
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(1e30f, 1.0f)), ==, 0x3fc90fdau);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-1e30f, -1.0f)), ==, 0xbfc90fdbu);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(1e-30f, 1.0f)), ==, 0x0da24260u);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(1e-30f, -1.0f)), ==, 0x40490fdbu);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(-1e-30f, -1.0f)), ==, 0xc0490fdbu);
    RC_CHECK(rc_bitcast_f32(rc_atan2_f32(0x1p-149f, 0x1p-149f)), ==, 0x3f490fdbu);
}


RC_TEST(math_atan2, f64)
{
    double infinity = rc_bitcast_u64(0x7ff0000000000000ull);

    // One in each quadrant, and x = 1 as plain atan
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(3.0, 4.0)), ==, 0x3fe4978fa3269ee1ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(3.0, -4.0)), ==, 0x4003fc176b7a8560ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-3.0, -4.0)), ==, 0xc003fc176b7a8560ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-3.0, 4.0)), ==, 0xbfe4978fa3269ee1ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-0.7, 1.0)), ==, 0xbfe38b112d7bd4adull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(1e-3, -2.5)), ==, 0x400921299d2d8fddull);

    // Both zeros against both zeros, and zero y against finite x
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(0.0, 0.0)), ==, 0x0000000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-0.0, 0.0)), ==, 0x8000000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(0.0, -0.0)), ==, 0x400921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-0.0, -0.0)), ==, 0xc00921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(0.0, 5.0)), ==, 0x0000000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-0.0, -5.0)), ==, 0xc00921fb54442d18ull);

    // Zero x, +-pi/2
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(3.0, 0.0)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-3.0, -0.0)), ==, 0xbff921fb54442d18ull);

    // Infinities against infinities, and against finite values
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(infinity, infinity)), ==, 0x3fe921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-infinity, infinity)), ==, 0xbfe921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(infinity, -infinity)), ==, 0x4002d97c7f3321d2ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-infinity, -infinity)), ==, 0xc002d97c7f3321d2ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(1.0, infinity)), ==, 0x0000000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-1.0, infinity)), ==, 0x8000000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(1.0, -infinity)), ==, 0x400921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-1.0, -infinity)), ==, 0xc00921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(infinity, 1.0)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-infinity, -1.0)), ==, 0xbff921fb54442d18ull);

    // Steep, beyond 2^64, and shallow, below 2^-64, on each side
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(1e30, 1.0)), ==, 0x3ff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-1e30, -1.0)), ==, 0xbff921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(1e-30, 1.0)), ==, 0x39b4484bfeebc2a0ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(1e-30, -1.0)), ==, 0x400921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(-1e-30, -1.0)), ==, 0xc00921fb54442d18ull);
    RC_CHECK(rc_bitcast_f64(rc_atan2_f64(0x1p-1074, 0x1p-1074)), ==, 0x3fe921fb54442d18ull);
}


RC_TEST(math_atan2, nan)
{
    float infinity_f32 = rc_bitcast_u32(0x7f800000u);
    float nan_f32 = rc_bitcast_u32(0x7fc00000u);
    double infinity_f64 = rc_bitcast_u64(0x7ff0000000000000ull);
    double nan_f64 = rc_bitcast_u64(0x7ff8000000000000ull);
    float nan_y_f32 = rc_atan2_f32(nan_f32, 1.0f);
    float nan_x_f32 = rc_atan2_f32(1.0f, nan_f32);
    float nan_both_f32 = rc_atan2_f32(nan_f32, nan_f32);
    double nan_y_f64 = rc_atan2_f64(nan_f64, 1.0);
    double nan_x_f64 = rc_atan2_f64(1.0, nan_f64);
    double nan_both_f64 = rc_atan2_f64(nan_f64, nan_f64);

    // NaN in either argument wins over everything, zeros and infinities included
    RC_CHECK_TRUE(nan_y_f32 != nan_y_f32);
    RC_CHECK_TRUE(nan_x_f32 != nan_x_f32);
    RC_CHECK_TRUE(nan_both_f32 != nan_both_f32);
    RC_CHECK_TRUE(nan_y_f64 != nan_y_f64);
    RC_CHECK_TRUE(nan_x_f64 != nan_x_f64);
    RC_CHECK_TRUE(nan_both_f64 != nan_both_f64);
    RC_CHECK_FALSE(rc_is_finite_f32(rc_atan2_f32(nan_f32, 0.0f)));
    RC_CHECK_FALSE(rc_is_finite_f32(rc_atan2_f32(-0.0f, nan_f32)));
    RC_CHECK_FALSE(rc_is_finite_f32(rc_atan2_f32(nan_f32, -infinity_f32)));
    RC_CHECK_FALSE(rc_is_finite_f32(rc_atan2_f32(infinity_f32, nan_f32)));
    RC_CHECK_FALSE(rc_is_finite_f64(rc_atan2_f64(nan_f64, 0.0)));
    RC_CHECK_FALSE(rc_is_finite_f64(rc_atan2_f64(-0.0, nan_f64)));
    RC_CHECK_FALSE(rc_is_finite_f64(rc_atan2_f64(nan_f64, -infinity_f64)));
    RC_CHECK_FALSE(rc_is_finite_f64(rc_atan2_f64(infinity_f64, nan_f64)));
}
