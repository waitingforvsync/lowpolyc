// test_math_trig.c
//
// Tests for rc_sin, rc_cos and rc_tan. The golden bit patterns come from our port, which gives
// musl's bits on every float and on a large sample of doubles; checking them pins down the same
// bits on every target. Inputs and outputs are written as bit patterns.

#include "richc/math/math.h"

#include "richc/test.h"

// True if x is of the given type, to see which width a dispatcher picked.
#define IS_TYPE(x, type) _Generic((x), type: true, default: false)

// The bits of sin(x), for x given by its bits.
static uint32_t sin_f32_bits(uint32_t x)
{
    return rc_bitcast_f32(rc_sin_f32(rc_bitcast_u32(x)));
}

// The bits of sin(x), for x given by its bits.
static uint64_t sin_f64_bits(uint64_t x)
{
    return rc_bitcast_f64(rc_sin_f64(rc_bitcast_u64(x)));
}

// The bits of cos(x), for x given by its bits.
static uint32_t cos_f32_bits(uint32_t x)
{
    return rc_bitcast_f32(rc_cos_f32(rc_bitcast_u32(x)));
}

// The bits of cos(x), for x given by its bits.
static uint64_t cos_f64_bits(uint64_t x)
{
    return rc_bitcast_f64(rc_cos_f64(rc_bitcast_u64(x)));
}

// The bits of tan(x), for x given by its bits.
static uint32_t tan_f32_bits(uint32_t x)
{
    return rc_bitcast_f32(rc_tan_f32(rc_bitcast_u32(x)));
}

// The bits of tan(x), for x given by its bits.
static uint64_t tan_f64_bits(uint64_t x)
{
    return rc_bitcast_f64(rc_tan_f64(rc_bitcast_u64(x)));
}


// ---- sin ----

RC_TEST(math_sin, values)
{
    RC_CHECK(rc_sin_f32(0.0f), ==, 0.0f);
    RC_CHECK(rc_sin_f64(0.0), ==, 0.0);
    RC_CHECK(rc_bitcast_f32(rc_sin_f32(-0.0f)), ==, 0x80000000u);
    RC_CHECK(rc_bitcast_f64(rc_sin_f64(-0.0)), ==, 0x8000000000000000ull);

    // Tiny angles are their own sine
    RC_CHECK(rc_sin_f32(1e-5f), ==, 1e-5f);
    RC_CHECK(rc_sin_f64(1e-10), ==, 1e-10);

    // sin(pi/6) is a half, give or take the rounding of pi/6
    RC_CHECK_TRUE(rc_abs_f64(rc_sin_f64(0.52359877559829887) - 0.5) < 1e-15);
    RC_CHECK_TRUE(rc_abs_f32(rc_sin_f32(0.52359878f) - 0.5f) < 1e-7f);

    // Odd, exactly
    RC_CHECK(rc_sin_f32(-2.5f), ==, -rc_sin_f32(2.5f));
    RC_CHECK(rc_sin_f64(-2.5), ==, -rc_sin_f64(2.5));
    RC_CHECK(rc_sin_f64(-1e30), ==, -rc_sin_f64(1e30));

    // The dispatcher picks the width of its argument
    RC_CHECK_TRUE(IS_TYPE(rc_sin(1.0f), float));
    RC_CHECK_TRUE(IS_TYPE(rc_sin(1.0), double));
    RC_CHECK(rc_sin(2.5f), ==, rc_sin_f32(2.5f));
    RC_CHECK(rc_sin(2.5), ==, rc_sin_f64(2.5));
}


RC_TEST(math_sin, f32_bits)
{
    // Zeros keep their sign, and below 2^-12 sin and tan give x back and cos 1
    RC_CHECK(sin_f32_bits(0x00000000u), ==, 0x00000000u);
    RC_CHECK(sin_f32_bits(0x80000000u), ==, 0x80000000u);
    RC_CHECK(sin_f32_bits(0x00000001u), ==, 0x00000001u);
    RC_CHECK(sin_f32_bits(0x80800000u), ==, 0x80800000u);
    RC_CHECK(sin_f32_bits(0x397fffffu), ==, 0x397fffffu);
    RC_CHECK(sin_f32_bits(0x39800000u), ==, 0x39800000u);

    // Within pi/4, and ordinary values
    RC_CHECK(sin_f32_bits(0x3f000000u), ==, 0x3ef57744u);
    RC_CHECK(sin_f32_bits(0x3f800000u), ==, 0x3f576aa4u);
    RC_CHECK(sin_f32_bits(0xbf800000u), ==, 0xbf576aa4u);
    RC_CHECK(sin_f32_bits(0x3f490fdau), ==, 0x3f3504f3u);
    RC_CHECK(sin_f32_bits(0x3f490fdbu), ==, 0x3f3504f3u);

    // Up to 9pi/4, reduced right there: near pi/2, pi, 3pi/2 and 2pi, and between
    RC_CHECK(sin_f32_bits(0x3fc90fdbu), ==, 0x3f800000u);
    RC_CHECK(sin_f32_bits(0xbfc90fdbu), ==, 0xbf800000u);
    RC_CHECK(sin_f32_bits(0x40000000u), ==, 0x3f68c7b7u);
    RC_CHECK(sin_f32_bits(0x40490fdbu), ==, 0xb3bbbd2eu);
    RC_CHECK(sin_f32_bits(0xc0400000u), ==, 0xbe1081c3u);
    RC_CHECK(sin_f32_bits(0x4096cbe4u), ==, 0xbf800000u);
    RC_CHECK(sin_f32_bits(0x40a00000u), ==, 0xbf757c10u);
    RC_CHECK(sin_f32_bits(0xc0a00000u), ==, 0x3f757c10u);
    RC_CHECK(sin_f32_bits(0x40c90fdbu), ==, 0x343bbd2eu);
    RC_CHECK(sin_f32_bits(0x40c00000u), ==, 0xbe8f0f8cu);
    RC_CHECK(sin_f32_bits(0xc0c00000u), ==, 0x3e8f0f8cu);

    // The medium reduction, up to just under 2^28 pi/2
    RC_CHECK(sin_f32_bits(0x41200000u), ==, 0xbf0b44f8u);
    RC_CHECK(sin_f32_bits(0x42c80000u), ==, 0xbf01a12eu);
    RC_CHECK(sin_f32_bits(0xc47a0000u), ==, 0xbf53ae61u);
    RC_CHECK(sin_f32_bits(0x4640e6b7u), ==, 0xbf341d93u);
    RC_CHECK(sin_f32_bits(0x4dc90fdau), ==, 0xbf7cf804u);

    // The large reduction, including one that brings in more of 2/pi, one rounded up from the
    // fraction, and ones whose quadrant bits sit in the chunks
    RC_CHECK(sin_f32_bits(0x4dc90fdbu), ==, 0xbf3d5f98u);
    RC_CHECK(sin_f32_bits(0x4dc943edu), ==, 0xb63d5d7fu);
    RC_CHECK(sin_f32_bits(0x4dc90fdcu), ==, 0xbe7bf669u);
    RC_CHECK(sin_f32_bits(0x57000000u), ==, 0xbd84e24au);
    RC_CHECK(sin_f32_bits(0x57800000u), ==, 0x3e049a9bu);
    RC_CHECK(sin_f32_bits(0x501502f9u), ==, 0xbef99a64u);
    RC_CHECK(sin_f32_bits(0x7149f2cau), ==, 0xbf4a89b0u);
    RC_CHECK(sin_f32_bits(0x7f7fffffu), ==, 0xbf0599b3u);
    RC_CHECK(sin_f32_bits(0xff7fffffu), ==, 0x3f0599b3u);
}


RC_TEST(math_sin, f64_bits)
{
    // Zeros keep their sign, and below 2^-26 (sin), 2^-27 sqrt(2) (cos) and 2^-27 (tan) we skip
    // the kernel
    RC_CHECK(sin_f64_bits(0x0000000000000000ull), ==, 0x0000000000000000ull);
    RC_CHECK(sin_f64_bits(0x8000000000000000ull), ==, 0x8000000000000000ull);
    RC_CHECK(sin_f64_bits(0x0000000000000001ull), ==, 0x0000000000000001ull);
    RC_CHECK(sin_f64_bits(0x3e4fffffffffffffull), ==, 0x3e4fffffffffffffull);
    RC_CHECK(sin_f64_bits(0x3e50000000000000ull), ==, 0x3e50000000000000ull);
    RC_CHECK(sin_f64_bits(0x3e46a09dffffffffull), ==, 0x3e46a09dffffffffull);
    RC_CHECK(sin_f64_bits(0x3e46a09e00000000ull), ==, 0x3e46a09e00000000ull);
    RC_CHECK(sin_f64_bits(0x3e3fffffffffffffull), ==, 0x3e3fffffffffffffull);
    RC_CHECK(sin_f64_bits(0x3e40000000000000ull), ==, 0x3e40000000000000ull);

    // Within pi/4, including either side of 0.6744, where the tan kernel changes formula
    RC_CHECK(sin_f64_bits(0x3fe0000000000000ull), ==, 0x3fdeaee8744b05f0ull);
    RC_CHECK(sin_f64_bits(0x3ff0000000000000ull), ==, 0x3feaed548f090ceeull);
    RC_CHECK(sin_f64_bits(0x3fe6666666666666ull), ==, 0x3fe49d6e694619b8ull);
    RC_CHECK(sin_f64_bits(0xbfe6666666666666ull), ==, 0xbfe49d6e694619b8ull);
    RC_CHECK(sin_f64_bits(0x3fe921fb54442d18ull), ==, 0x3fe6a09e667f3bccull);
    RC_CHECK(sin_f64_bits(0x3fe921fc00000000ull), ==, 0x3fe6a09edfee58baull);

    // Multiples of pi/2 up to 2pi, which need the medium reduction (pi/2 three rounds of it), and
    // the one-step reductions between them
    RC_CHECK(sin_f64_bits(0x3ff921fb54442d18ull), ==, 0x3ff0000000000000ull);
    RC_CHECK(sin_f64_bits(0xbff921fb54442d18ull), ==, 0xbff0000000000000ull);
    RC_CHECK(sin_f64_bits(0x400921fb54442d18ull), ==, 0x3ca1a62633145c07ull);
    RC_CHECK(sin_f64_bits(0x4012d97c7f3321d2ull), ==, 0xbff0000000000000ull);
    RC_CHECK(sin_f64_bits(0x401921fb54442d18ull), ==, 0xbcb1a62633145c07ull);
    RC_CHECK(sin_f64_bits(0x4000000000000000ull), ==, 0x3fed18f6ead1b446ull);
    RC_CHECK(sin_f64_bits(0x4008000000000000ull), ==, 0x3fc210386db6d55bull);
    RC_CHECK(sin_f64_bits(0x4014000000000000ull), ==, 0xbfeeaf81f5e09933ull);
    RC_CHECK(sin_f64_bits(0x401c000000000000ull), ==, 0x3fe50608c26d0a08ull);
    RC_CHECK(sin_f64_bits(0xc01c000000000000ull), ==, 0xbfe50608c26d0a08ull);

    // The medium reduction, up to just under 2^20 pi/2
    RC_CHECK(sin_f64_bits(0x4024000000000000ull), ==, 0xbfe1689ef5f34f52ull);
    RC_CHECK(sin_f64_bits(0x40f86a0000000000ull), ==, 0x3fa24daa9c527e96ull);
    RC_CHECK(sin_f64_bits(0xc0fe240c9fbe76c9ull), ==, 0x3feff50e60ab53f9ull);
    RC_CHECK(sin_f64_bits(0x413921faffffffffull), ==, 0xbfd4b02e5c25af00ull);

    // The large reduction, including the double closest to a multiple of pi/2, one that brings in
    // more of 2/pi, one rounded up from the fraction, ones whose quadrant bits sit in the chunks,
    // and one whose remainder spills into a second chunk
    RC_CHECK(sin_f64_bits(0x413921fb00000000ull), ==, 0xbfd4b02e5be91e9eull);
    RC_CHECK(sin_f64_bits(0x41413db55a439968ull), ==, 0x3fed2bdce3b7cbbbull);
    RC_CHECK(sin_f64_bits(0x4480f0cf064dd592ull), ==, 0xbfeb453ab76bf397ull);
    RC_CHECK(sin_f64_bits(0x46293e5939a08ceaull), ==, 0x3f831c608f107767ull);
    RC_CHECK(sin_f64_bits(0x7506ac5b262ca1ffull), ==, 0x3ff0000000000000ull);
    RC_CHECK(sin_f64_bits(0x69efe333b266f103ull), ==, 0xbfed223d4db96e8eull);
    RC_CHECK(sin_f64_bits(0x6f525307d4673f9full), ==, 0xbfeab07a58c25de8ull);
    RC_CHECK(sin_f64_bits(0x7bf741acd442110dull), ==, 0x3f9e236aeafc1375ull);
    RC_CHECK(sin_f64_bits(0x7d6ce98dd00116fcull), ==, 0x3fef466aa1c97d5eull);
    RC_CHECK(sin_f64_bits(0x7fefffffffffffffull), ==, 0x3f7452fc98b34e97ull);
    RC_CHECK(sin_f64_bits(0xffefffffffffffffull), ==, 0xbf7452fc98b34e97ull);
}


// Infinities and NaN all give NaN
RC_TEST(math_sin, not_finite)
{
    float values_f32[] = {
        rc_sin_f32(rc_bitcast_u32(0x7f800000u)),
        rc_sin_f32(rc_bitcast_u32(0xff800000u)),
        rc_sin_f32(rc_bitcast_u32(0x7fc00000u)),
        rc_sin_f32(rc_bitcast_u32(0xffc00001u))
    };
    double values_f64[] = {
        rc_sin_f64(rc_bitcast_u64(0x7ff0000000000000ull)),
        rc_sin_f64(rc_bitcast_u64(0xfff0000000000000ull)),
        rc_sin_f64(rc_bitcast_u64(0x7ff8000000000000ull)),
        rc_sin_f64(rc_bitcast_u64(0xfff8000000000001ull))
    };

    for (uint32_t i = 0; i < RC_COUNT(values_f32); i++) {
        RC_CHECK_FALSE(rc_is_finite_f32(values_f32[i]));
        RC_CHECK_TRUE(values_f32[i] != values_f32[i]);
    }

    for (uint32_t i = 0; i < RC_COUNT(values_f64); i++) {
        RC_CHECK_FALSE(rc_is_finite_f64(values_f64[i]));
        RC_CHECK_TRUE(values_f64[i] != values_f64[i]);
    }
}

// ---- cos ----

RC_TEST(math_cos, values)
{
    RC_CHECK(rc_cos_f32(0.0f), ==, 1.0f);
    RC_CHECK(rc_cos_f64(0.0), ==, 1.0);
    RC_CHECK(rc_cos_f32(-0.0f), ==, 1.0f);
    RC_CHECK(rc_cos_f64(-0.0), ==, 1.0);

    // Tiny angles have a cosine of exactly 1
    RC_CHECK(rc_cos_f32(1e-5f), ==, 1.0f);
    RC_CHECK(rc_cos_f64(1e-10), ==, 1.0);

    // cos(pi/3) is a half, give or take the rounding of pi/3
    RC_CHECK_TRUE(rc_abs_f64(rc_cos_f64(1.0471975511965976) - 0.5) < 1e-15);
    RC_CHECK_TRUE(rc_abs_f32(rc_cos_f32(1.0471976f) - 0.5f) < 1e-7f);

    // Even, exactly
    RC_CHECK(rc_cos_f32(-2.5f), ==, rc_cos_f32(2.5f));
    RC_CHECK(rc_cos_f64(-2.5), ==, rc_cos_f64(2.5));
    RC_CHECK(rc_cos_f64(-1e30), ==, rc_cos_f64(1e30));

    // The dispatcher picks the width of its argument
    RC_CHECK_TRUE(IS_TYPE(rc_cos(1.0f), float));
    RC_CHECK_TRUE(IS_TYPE(rc_cos(1.0), double));
    RC_CHECK(rc_cos(2.5f), ==, rc_cos_f32(2.5f));
    RC_CHECK(rc_cos(2.5), ==, rc_cos_f64(2.5));
}


RC_TEST(math_cos, f32_bits)
{
    // Zeros keep their sign, and below 2^-12 sin and tan give x back and cos 1
    RC_CHECK(cos_f32_bits(0x00000000u), ==, 0x3f800000u);
    RC_CHECK(cos_f32_bits(0x80000000u), ==, 0x3f800000u);
    RC_CHECK(cos_f32_bits(0x00000001u), ==, 0x3f800000u);
    RC_CHECK(cos_f32_bits(0x80800000u), ==, 0x3f800000u);
    RC_CHECK(cos_f32_bits(0x397fffffu), ==, 0x3f800000u);
    RC_CHECK(cos_f32_bits(0x39800000u), ==, 0x3f800000u);

    // Within pi/4, and ordinary values
    RC_CHECK(cos_f32_bits(0x3f000000u), ==, 0x3f60a940u);
    RC_CHECK(cos_f32_bits(0x3f800000u), ==, 0x3f0a5140u);
    RC_CHECK(cos_f32_bits(0xbf800000u), ==, 0x3f0a5140u);
    RC_CHECK(cos_f32_bits(0x3f490fdau), ==, 0x3f3504f4u);
    RC_CHECK(cos_f32_bits(0x3f490fdbu), ==, 0x3f3504f3u);

    // Up to 9pi/4, reduced right there: near pi/2, pi, 3pi/2 and 2pi, and between
    RC_CHECK(cos_f32_bits(0x3fc90fdbu), ==, 0xb33bbd2eu);
    RC_CHECK(cos_f32_bits(0xbfc90fdbu), ==, 0xb33bbd2eu);
    RC_CHECK(cos_f32_bits(0x40000000u), ==, 0xbed51133u);
    RC_CHECK(cos_f32_bits(0x40490fdbu), ==, 0xbf800000u);
    RC_CHECK(cos_f32_bits(0xc0400000u), ==, 0xbf7d7026u);
    RC_CHECK(cos_f32_bits(0x4096cbe4u), ==, 0x324cde2eu);
    RC_CHECK(cos_f32_bits(0x40a00000u), ==, 0x3e913c2cu);
    RC_CHECK(cos_f32_bits(0xc0a00000u), ==, 0x3e913c2cu);
    RC_CHECK(cos_f32_bits(0x40c90fdbu), ==, 0x3f800000u);
    RC_CHECK(cos_f32_bits(0x40c00000u), ==, 0x3f75cdb8u);
    RC_CHECK(cos_f32_bits(0xc0c00000u), ==, 0x3f75cdb8u);

    // The medium reduction, up to just under 2^28 pi/2
    RC_CHECK(cos_f32_bits(0x41200000u), ==, 0xbf56cd64u);
    RC_CHECK(cos_f32_bits(0x42c80000u), ==, 0x3f5cc0eeu);
    RC_CHECK(cos_f32_bits(0xc47a0000u), ==, 0x3f0ff813u);
    RC_CHECK(cos_f32_bits(0x4640e6b7u), ==, 0x3f35eb2du);
    RC_CHECK(cos_f32_bits(0x4dc90fdau), ==, 0x3e1d1cf0u);

    // The large reduction, including one that brings in more of 2/pi, one rounded up from the
    // fraction, and ones whose quadrant bits sit in the chunks
    RC_CHECK(cos_f32_bits(0x4dc90fdbu), ==, 0x3f2c42b6u);
    RC_CHECK(cos_f32_bits(0x4dc943edu), ==, 0x3f800000u);
    RC_CHECK(cos_f32_bits(0x4dc90fdcu), ==, 0x3f78211eu);
    RC_CHECK(cos_f32_bits(0x57000000u), ==, 0xbf7f75e6u);
    RC_CHECK(cos_f32_bits(0x57800000u), ==, 0x3f7dd82fu);
    RC_CHECK(cos_f32_bits(0x501502f9u), ==, 0x3f5f84c5u);
    RC_CHECK(cos_f32_bits(0x7149f2cau), ==, 0xbf1c9222u);
    RC_CHECK(cos_f32_bits(0x7f7fffffu), ==, 0x3f5a5f96u);
    RC_CHECK(cos_f32_bits(0xff7fffffu), ==, 0x3f5a5f96u);
}


RC_TEST(math_cos, f64_bits)
{
    // Zeros keep their sign, and below 2^-26 (sin), 2^-27 sqrt(2) (cos) and 2^-27 (tan) we skip
    // the kernel
    RC_CHECK(cos_f64_bits(0x0000000000000000ull), ==, 0x3ff0000000000000ull);
    RC_CHECK(cos_f64_bits(0x8000000000000000ull), ==, 0x3ff0000000000000ull);
    RC_CHECK(cos_f64_bits(0x0000000000000001ull), ==, 0x3ff0000000000000ull);
    RC_CHECK(cos_f64_bits(0x3e4fffffffffffffull), ==, 0x3fefffffffffffffull);
    RC_CHECK(cos_f64_bits(0x3e50000000000000ull), ==, 0x3fefffffffffffffull);
    RC_CHECK(cos_f64_bits(0x3e46a09dffffffffull), ==, 0x3ff0000000000000ull);
    RC_CHECK(cos_f64_bits(0x3e46a09e00000000ull), ==, 0x3ff0000000000000ull);
    RC_CHECK(cos_f64_bits(0x3e3fffffffffffffull), ==, 0x3ff0000000000000ull);
    RC_CHECK(cos_f64_bits(0x3e40000000000000ull), ==, 0x3ff0000000000000ull);

    // Within pi/4, including either side of 0.6744, where the tan kernel changes formula
    RC_CHECK(cos_f64_bits(0x3fe0000000000000ull), ==, 0x3fec1528065b7d50ull);
    RC_CHECK(cos_f64_bits(0x3ff0000000000000ull), ==, 0x3fe14a280fb5068cull);
    RC_CHECK(cos_f64_bits(0x3fe6666666666666ull), ==, 0x3fe87996529f9d93ull);
    RC_CHECK(cos_f64_bits(0xbfe6666666666666ull), ==, 0x3fe87996529f9d93ull);
    RC_CHECK(cos_f64_bits(0x3fe921fb54442d18ull), ==, 0x3fe6a09e667f3bcdull);
    RC_CHECK(cos_f64_bits(0x3fe921fc00000000ull), ==, 0x3fe6a09ded101c54ull);

    // Multiples of pi/2 up to 2pi, which need the medium reduction (pi/2 three rounds of it), and
    // the one-step reductions between them
    RC_CHECK(cos_f64_bits(0x3ff921fb54442d18ull), ==, 0x3c91a62633145c07ull);
    RC_CHECK(cos_f64_bits(0xbff921fb54442d18ull), ==, 0x3c91a62633145c07ull);
    RC_CHECK(cos_f64_bits(0x400921fb54442d18ull), ==, 0xbff0000000000000ull);
    RC_CHECK(cos_f64_bits(0x4012d97c7f3321d2ull), ==, 0xbcaa79394c9e8a0aull);
    RC_CHECK(cos_f64_bits(0x401921fb54442d18ull), ==, 0x3ff0000000000000ull);
    RC_CHECK(cos_f64_bits(0x4000000000000000ull), ==, 0xbfdaa22657537205ull);
    RC_CHECK(cos_f64_bits(0x4008000000000000ull), ==, 0xbfefae04be85e5d2ull);
    RC_CHECK(cos_f64_bits(0x4014000000000000ull), ==, 0x3fd22785706b4ad9ull);
    RC_CHECK(cos_f64_bits(0x401c000000000000ull), ==, 0x3fe81ff79ed92017ull);
    RC_CHECK(cos_f64_bits(0xc01c000000000000ull), ==, 0x3fe81ff79ed92017ull);

    // The medium reduction, up to just under 2^20 pi/2
    RC_CHECK(cos_f64_bits(0x4024000000000000ull), ==, 0xbfead9ac890c6b1full);
    RC_CHECK(cos_f64_bits(0x40f86a0000000000ull), ==, 0xbfeffac3841b3da7ull);
    RC_CHECK(cos_f64_bits(0xc0fe240c9fbe76c9ull), ==, 0x3faa74d27c41b22aull);
    RC_CHECK(cos_f64_bits(0x413921faffffffffull), ==, 0x3fee483125700ac3ull);

    // The large reduction, including the double closest to a multiple of pi/2, one that brings in
    // more of 2/pi, one rounded up from the fraction, ones whose quadrant bits sit in the chunks,
    // and one whose remainder spills into a second chunk
    RC_CHECK(cos_f64_bits(0x413921fb00000000ull), ==, 0x3fee4831257a62daull);
    RC_CHECK(cos_f64_bits(0x41413db55a439968ull), ==, 0xbfda4ef33e8901f6ull);
    RC_CHECK(cos_f64_bits(0x4480f0cf064dd592ull), ==, 0x3fe0be2cef01c8f4ull);
    RC_CHECK(cos_f64_bits(0x46293e5939a08ceaull), ==, 0xbfefffa4b11f1b45ull);
    RC_CHECK(cos_f64_bits(0x7506ac5b262ca1ffull), ==, 0xbc214ae72e6ba22full);
    RC_CHECK(cos_f64_bits(0x69efe333b266f103ull), ==, 0xbfda79788ed302d8ull);
    RC_CHECK(cos_f64_bits(0x6f525307d4673f9full), ==, 0xbfe1a7867e40ef8eull);
    RC_CHECK(cos_f64_bits(0x7bf741acd442110dull), ==, 0xbfeffc737ba997deull);
    RC_CHECK(cos_f64_bits(0x7d6ce98dd00116fcull), ==, 0x3fcb174f8431ba60ull);
    RC_CHECK(cos_f64_bits(0x7fefffffffffffffull), ==, 0xbfefffe62ecfab75ull);
    RC_CHECK(cos_f64_bits(0xffefffffffffffffull), ==, 0xbfefffe62ecfab75ull);
}


// Infinities and NaN all give NaN
RC_TEST(math_cos, not_finite)
{
    float values_f32[] = {
        rc_cos_f32(rc_bitcast_u32(0x7f800000u)),
        rc_cos_f32(rc_bitcast_u32(0xff800000u)),
        rc_cos_f32(rc_bitcast_u32(0x7fc00000u)),
        rc_cos_f32(rc_bitcast_u32(0xffc00001u))
    };
    double values_f64[] = {
        rc_cos_f64(rc_bitcast_u64(0x7ff0000000000000ull)),
        rc_cos_f64(rc_bitcast_u64(0xfff0000000000000ull)),
        rc_cos_f64(rc_bitcast_u64(0x7ff8000000000000ull)),
        rc_cos_f64(rc_bitcast_u64(0xfff8000000000001ull))
    };

    for (uint32_t i = 0; i < RC_COUNT(values_f32); i++) {
        RC_CHECK_FALSE(rc_is_finite_f32(values_f32[i]));
        RC_CHECK_TRUE(values_f32[i] != values_f32[i]);
    }

    for (uint32_t i = 0; i < RC_COUNT(values_f64); i++) {
        RC_CHECK_FALSE(rc_is_finite_f64(values_f64[i]));
        RC_CHECK_TRUE(values_f64[i] != values_f64[i]);
    }
}

// ---- tan ----

RC_TEST(math_tan, values)
{
    RC_CHECK(rc_tan_f32(0.0f), ==, 0.0f);
    RC_CHECK(rc_tan_f64(0.0), ==, 0.0);
    RC_CHECK(rc_bitcast_f32(rc_tan_f32(-0.0f)), ==, 0x80000000u);
    RC_CHECK(rc_bitcast_f64(rc_tan_f64(-0.0)), ==, 0x8000000000000000ull);

    // Tiny angles are their own tangent
    RC_CHECK(rc_tan_f32(1e-5f), ==, 1e-5f);
    RC_CHECK(rc_tan_f64(1e-10), ==, 1e-10);

    // tan(pi/4) is 1, give or take the rounding of pi/4
    RC_CHECK_TRUE(rc_abs_f64(rc_tan_f64(0.78539816339744831) - 1.0) < 1e-15);
    RC_CHECK_TRUE(rc_abs_f32(rc_tan_f32(0.78539819f) - 1.0f) < 1e-6f);

    // Odd, exactly
    RC_CHECK(rc_tan_f32(-2.5f), ==, -rc_tan_f32(2.5f));
    RC_CHECK(rc_tan_f64(-2.5), ==, -rc_tan_f64(2.5));
    RC_CHECK(rc_tan_f64(-1e30), ==, -rc_tan_f64(1e30));

    // The dispatcher picks the width of its argument
    RC_CHECK_TRUE(IS_TYPE(rc_tan(1.0f), float));
    RC_CHECK_TRUE(IS_TYPE(rc_tan(1.0), double));
    RC_CHECK(rc_tan(2.5f), ==, rc_tan_f32(2.5f));
    RC_CHECK(rc_tan(2.5), ==, rc_tan_f64(2.5));
}


RC_TEST(math_tan, f32_bits)
{
    // Zeros keep their sign, and below 2^-12 sin and tan give x back and cos 1
    RC_CHECK(tan_f32_bits(0x00000000u), ==, 0x00000000u);
    RC_CHECK(tan_f32_bits(0x80000000u), ==, 0x80000000u);
    RC_CHECK(tan_f32_bits(0x00000001u), ==, 0x00000001u);
    RC_CHECK(tan_f32_bits(0x80800000u), ==, 0x80800000u);
    RC_CHECK(tan_f32_bits(0x397fffffu), ==, 0x397fffffu);
    RC_CHECK(tan_f32_bits(0x39800000u), ==, 0x39800000u);

    // Within pi/4, and ordinary values
    RC_CHECK(tan_f32_bits(0x3f000000u), ==, 0x3f0bda7bu);
    RC_CHECK(tan_f32_bits(0x3f800000u), ==, 0x3fc75923u);
    RC_CHECK(tan_f32_bits(0xbf800000u), ==, 0xbfc75923u);
    RC_CHECK(tan_f32_bits(0x3f490fdau), ==, 0x3f7ffffeu);
    RC_CHECK(tan_f32_bits(0x3f490fdbu), ==, 0x3f800000u);

    // Up to 9pi/4, reduced right there: near pi/2, pi, 3pi/2 and 2pi, and between
    RC_CHECK(tan_f32_bits(0x3fc90fdbu), ==, 0xcbae8a4au);
    RC_CHECK(tan_f32_bits(0xbfc90fdbu), ==, 0x4bae8a4au);
    RC_CHECK(tan_f32_bits(0x40000000u), ==, 0xc00bd7b1u);
    RC_CHECK(tan_f32_bits(0x40490fdbu), ==, 0x33bbbd2fu);
    RC_CHECK(tan_f32_bits(0xc0400000u), ==, 0x3e11f7b8u);
    RC_CHECK(tan_f32_bits(0x4096cbe4u), ==, 0xcc9ff26du);
    RC_CHECK(tan_f32_bits(0x40a00000u), ==, 0xc0585a5bu);
    RC_CHECK(tan_f32_bits(0xc0a00000u), ==, 0x40585a5bu);
    RC_CHECK(tan_f32_bits(0x40c90fdbu), ==, 0x343bbd2fu);
    RC_CHECK(tan_f32_bits(0x40c00000u), ==, 0xbe94fec4u);
    RC_CHECK(tan_f32_bits(0xc0c00000u), ==, 0x3e94fec4u);

    // The medium reduction, up to just under 2^28 pi/2
    RC_CHECK(tan_f32_bits(0x41200000u), ==, 0x3f25fafau);
    RC_CHECK(tan_f32_bits(0x42c80000u), ==, 0xbf1653a7u);
    RC_CHECK(tan_f32_bits(0xc47a0000u), ==, 0xbfbc3395u);
    RC_CHECK(tan_f32_bits(0x4640e6b7u), ==, 0xbf7d766du);
    RC_CHECK(tan_f32_bits(0x4dc90fdau), ==, 0xc0ce17fdu);

    // The large reduction, including one that brings in more of 2/pi, one rounded up from the
    // fraction, and ones whose quadrant bits sit in the chunks
    RC_CHECK(tan_f32_bits(0x4dc90fdbu), ==, 0xbf8cb742u);
    RC_CHECK(tan_f32_bits(0x4dc943edu), ==, 0xb63d5d7fu);
    RC_CHECK(tan_f32_bits(0x4dc90fdcu), ==, 0xbe81fa36u);
    RC_CHECK(tan_f32_bits(0x57000000u), ==, 0x3d852a20u);
    RC_CHECK(tan_f32_bits(0x57800000u), ==, 0x3e05baddu);
    RC_CHECK(tan_f32_bits(0x501502f9u), ==, 0xbf0ef000u);
    RC_CHECK(tan_f32_bits(0x7149f2cau), ==, 0x3fa5943bu);
    RC_CHECK(tan_f32_bits(0x7f7fffffu), ==, 0xbf1c9ecau);
    RC_CHECK(tan_f32_bits(0xff7fffffu), ==, 0x3f1c9ecau);
}


RC_TEST(math_tan, f64_bits)
{
    // Zeros keep their sign, and below 2^-26 (sin), 2^-27 sqrt(2) (cos) and 2^-27 (tan) we skip
    // the kernel
    RC_CHECK(tan_f64_bits(0x0000000000000000ull), ==, 0x0000000000000000ull);
    RC_CHECK(tan_f64_bits(0x8000000000000000ull), ==, 0x8000000000000000ull);
    RC_CHECK(tan_f64_bits(0x0000000000000001ull), ==, 0x0000000000000001ull);
    RC_CHECK(tan_f64_bits(0x3e4fffffffffffffull), ==, 0x3e50000000000000ull);
    RC_CHECK(tan_f64_bits(0x3e50000000000000ull), ==, 0x3e50000000000000ull);
    RC_CHECK(tan_f64_bits(0x3e46a09dffffffffull), ==, 0x3e46a09dffffffffull);
    RC_CHECK(tan_f64_bits(0x3e46a09e00000000ull), ==, 0x3e46a09e00000000ull);
    RC_CHECK(tan_f64_bits(0x3e3fffffffffffffull), ==, 0x3e3fffffffffffffull);
    RC_CHECK(tan_f64_bits(0x3e40000000000000ull), ==, 0x3e40000000000000ull);

    // Within pi/4, including either side of 0.6744, where the tan kernel changes formula
    RC_CHECK(tan_f64_bits(0x3fe0000000000000ull), ==, 0x3fe17b4f5bf3474aull);
    RC_CHECK(tan_f64_bits(0x3ff0000000000000ull), ==, 0x3ff8eb245cbee3a6ull);
    RC_CHECK(tan_f64_bits(0x3fe6666666666666ull), ==, 0x3feaf406c2fc78aeull);
    RC_CHECK(tan_f64_bits(0xbfe6666666666666ull), ==, 0xbfeaf406c2fc78aeull);
    RC_CHECK(tan_f64_bits(0x3fe921fb54442d18ull), ==, 0x3fefffffffffffffull);
    RC_CHECK(tan_f64_bits(0x3fe921fc00000000ull), ==, 0x3ff00000abbbd681ull);

    // Multiples of pi/2 up to 2pi, which need the medium reduction (pi/2 three rounds of it), and
    // the one-step reductions between them
    RC_CHECK(tan_f64_bits(0x3ff921fb54442d18ull), ==, 0x434d02967c31cdb5ull);
    RC_CHECK(tan_f64_bits(0xbff921fb54442d18ull), ==, 0xc34d02967c31cdb5ull);
    RC_CHECK(tan_f64_bits(0x400921fb54442d18ull), ==, 0xbca1a62633145c07ull);
    RC_CHECK(tan_f64_bits(0x4012d97c7f3321d2ull), ==, 0x4333570efd768923ull);
    RC_CHECK(tan_f64_bits(0x401921fb54442d18ull), ==, 0xbcb1a62633145c07ull);
    RC_CHECK(tan_f64_bits(0x4000000000000000ull), ==, 0xc0017af62e0950f8ull);
    RC_CHECK(tan_f64_bits(0x4008000000000000ull), ==, 0xbfc23ef71254b86full);
    RC_CHECK(tan_f64_bits(0x4014000000000000ull), ==, 0xc00b0b4b739bbb07ull);
    RC_CHECK(tan_f64_bits(0x401c000000000000ull), ==, 0x3febe2e6e13eea78ull);
    RC_CHECK(tan_f64_bits(0xc01c000000000000ull), ==, 0xbfebe2e6e13eea78ull);

    // The medium reduction, up to just under 2^20 pi/2
    RC_CHECK(tan_f64_bits(0x4024000000000000ull), ==, 0x3fe4bf5f34be3782ull);
    RC_CHECK(tan_f64_bits(0x40f86a0000000000ull), ==, 0xbfa250a9d503313dull);
    RC_CHECK(tan_f64_bits(0xc0fe240c9fbe76c9ull), ==, 0x403353a85fe8d6d5ull);
    RC_CHECK(tan_f64_bits(0x413921faffffffffull), ==, 0xbfd5dca6bd5e8435ull);

    // The large reduction, including the double closest to a multiple of pi/2, one that brings in
    // more of 2/pi, one rounded up from the fraction, ones whose quadrant bits sit in the chunks,
    // and one whose remainder spills into a second chunk
    RC_CHECK(tan_f64_bits(0x413921fb00000000ull), ==, 0xbfd5dca6bd170c6full);
    RC_CHECK(tan_f64_bits(0x41413db55a439968ull), ==, 0xc001bdbc7013ebf7ull);
    RC_CHECK(tan_f64_bits(0x4480f0cf064dd592ull), ==, 0xbffa0f79c1b6b258ull);
    RC_CHECK(tan_f64_bits(0x46293e5939a08ceaull), ==, 0xbf831c97177a2330ull);
    RC_CHECK(tan_f64_bits(0x7506ac5b262ca1ffull), ==, 0xc3bd9ba9a7975636ull);
    RC_CHECK(tan_f64_bits(0x69efe333b266f103ull), ==, 0x40019b6d100b6482ull);
    RC_CHECK(tan_f64_bits(0x6f525307d4673f9full), ==, 0x3ff8303497c03470ull);
    RC_CHECK(tan_f64_bits(0x7bf741acd442110dull), ==, 0xbf9e26c2f389203full);
    RC_CHECK(tan_f64_bits(0x7d6ce98dd00116fcull), ==, 0x401278989462aa70ull);
    RC_CHECK(tan_f64_bits(0x7fefffffffffffffull), ==, 0xbf74530cfe729484ull);
    RC_CHECK(tan_f64_bits(0xffefffffffffffffull), ==, 0x3f74530cfe729484ull);
}


// Infinities and NaN all give NaN
RC_TEST(math_tan, not_finite)
{
    float values_f32[] = {
        rc_tan_f32(rc_bitcast_u32(0x7f800000u)),
        rc_tan_f32(rc_bitcast_u32(0xff800000u)),
        rc_tan_f32(rc_bitcast_u32(0x7fc00000u)),
        rc_tan_f32(rc_bitcast_u32(0xffc00001u))
    };
    double values_f64[] = {
        rc_tan_f64(rc_bitcast_u64(0x7ff0000000000000ull)),
        rc_tan_f64(rc_bitcast_u64(0xfff0000000000000ull)),
        rc_tan_f64(rc_bitcast_u64(0x7ff8000000000000ull)),
        rc_tan_f64(rc_bitcast_u64(0xfff8000000000001ull))
    };

    for (uint32_t i = 0; i < RC_COUNT(values_f32); i++) {
        RC_CHECK_FALSE(rc_is_finite_f32(values_f32[i]));
        RC_CHECK_TRUE(values_f32[i] != values_f32[i]);
    }

    for (uint32_t i = 0; i < RC_COUNT(values_f64); i++) {
        RC_CHECK_FALSE(rc_is_finite_f64(values_f64[i]));
        RC_CHECK_TRUE(values_f64[i] != values_f64[i]);
    }
}
