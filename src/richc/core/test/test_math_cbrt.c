// test_math_cbrt.c
//
// Tests for rc_cbrt_f32 and rc_cbrt_f64. The expected bits are golden values, recorded from the
// port once it matched musl on every float input and on hundreds of millions of doubles, so every
// target must give exactly these.

#include "richc/math/math.h"

#include "richc/test.h"

RC_TEST(math_cbrt, known_values)
{
    RC_CHECK(rc_cbrt_f32(0.0f), ==, 0.0f);
    RC_CHECK(rc_cbrt_f32(1.0f), ==, 1.0f);
    RC_CHECK(rc_cbrt_f32(8.0f), ==, 2.0f);
    RC_CHECK(rc_cbrt_f32(27.0f), ==, 3.0f);
    RC_CHECK(rc_cbrt_f32(-64.0f), ==, -4.0f);
    RC_CHECK(rc_cbrt_f32(0.125f), ==, 0.5f);
    RC_CHECK(rc_cbrt_f64(0.0), ==, 0.0);
    RC_CHECK(rc_cbrt_f64(1.0), ==, 1.0);
    RC_CHECK(rc_cbrt_f64(-8.0), ==, -2.0);
    RC_CHECK(rc_cbrt_f64(1000.0), ==, 10.0);
    RC_CHECK(rc_cbrt_f64(0x1p-1074), ==, 0x1p-358);
}


RC_TEST(math_cbrt, f32)
{
    float infinity = rc_bitcast_u32(0x7f800000u);

    // Zeros and infinities are their own roots
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(0.0f)), ==, 0x00000000u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(-0.0f)), ==, 0x80000000u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(infinity)), ==, 0x7f800000u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(-infinity)), ==, 0xff800000u);

    // Subnormals, scaled up for the estimate
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(0x1p-149f)), ==, 0x26a14518u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(-0x1p-149f)), ==, 0xa6a14518u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(0x1.8p-140f)), ==, 0x28389ba2u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(0x1p-127f)), ==, 0x2a4b2ff5u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(0x1.fffffcp-127f)), ==, 0x2a7fffffu);

    // Normal numbers, across the range and each residue of the exponent mod 3
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(0x1p-126f)), ==, 0x2a800000u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(-1e-30f)), ==, 0xaedbe6ffu);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(0.001f)), ==, 0x3dcccccdu);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(0.1f)), ==, 0x3eeda63cu);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(1.0f)), ==, 0x3f800000u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(-1.0f)), ==, 0xbf800000u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(2.0f)), ==, 0x3fa14518u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(3.0f)), ==, 0x3fb89ba2u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(4.0f)), ==, 0x3fcb2ff5u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(-5.0f)), ==, 0xbfdae07eu);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(10.0f)), ==, 0x4009e242u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(0x1.fffffep0f)), ==, 0x3fa14518u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(12345.678f)), ==, 0x41b8e576u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(1e30f)), ==, 0x501502f9u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(0x1.fffffep127f)), ==, 0x54cb2ff5u);
    RC_CHECK(rc_bitcast_f32(rc_cbrt_f32(-0x1.fffffep127f)), ==, 0xd4cb2ff5u);
}


RC_TEST(math_cbrt, f64)
{
    double infinity = rc_bitcast_u64(0x7ff0000000000000ull);

    // Zeros and infinities are their own roots
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(0.0)), ==, 0x0000000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(-0.0)), ==, 0x8000000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(infinity)), ==, 0x7ff0000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(-infinity)), ==, 0xfff0000000000000ull);

    // Subnormals, scaled up for the estimate
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(0x1p-1074)), ==, 0x2990000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(-0x1p-1074)), ==, 0xa990000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(0x1.8p-1060)), ==, 0x29dd12ed0af1a27full);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(0x1p-1023)), ==, 0x2aa0000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(0x0.fffffffffffffp-1022)), ==, 0x2aa428a2f98d728aull);

    // Normal numbers, across the range and each residue of the exponent mod 3
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(0x1p-1022)), ==, 0x2aa428a2f98d728bull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(-1e-300)), ==, 0xab2bff2ee48e0530ull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(0.001)), ==, 0x3fb999999999999aull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(0.1)), ==, 0x3fddb4c7760bcff3ull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(1.0)), ==, 0x3ff0000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(-1.0)), ==, 0xbff0000000000000ull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(2.0)), ==, 0x3ff428a2f98d728bull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(3.0)), ==, 0x3ff7137449123ef6ull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(4.0)), ==, 0x3ff965fea53d6e3dull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(-5.0)), ==, 0xbffb5c0fbcfec4d4ull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(10.0)), ==, 0x40013c484138704full);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(0x1.fffffffffffffp0)), ==, 0x3ff428a2f98d728bull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(12345.678)), ==, 0x40371caec6430a06ull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(1e300)), ==, 0x54b249ad2594c37dull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(0x1.fffffffffffffp1023)), ==, 0x554428a2f98d728bull);
    RC_CHECK(rc_bitcast_f64(rc_cbrt_f64(-0x1.fffffffffffffp1023)), ==, 0xd54428a2f98d728bull);
}


RC_TEST(math_cbrt, nan)
{
    float nan_f32 = rc_cbrt_f32(rc_bitcast_u32(0x7fc00000u));
    double nan_f64 = rc_cbrt_f64(rc_bitcast_u64(0x7ff8000000000000ull));

    RC_CHECK_TRUE(nan_f32 != nan_f32);
    RC_CHECK_TRUE(nan_f64 != nan_f64);
    RC_CHECK_FALSE(rc_is_finite_f32(nan_f32));
    RC_CHECK_FALSE(rc_is_finite_f64(nan_f64));
}
