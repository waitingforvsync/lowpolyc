// test_math_exp_log_f32.c
//
// Tests for rc_exp_f32, rc_log_f32, rc_log2_f32 and rc_pow_f32: every special value C17's Annex F
// gives them, the exact results the accuracy limits demand, overflow and underflow, subnormals,
// and golden values. The golden values are the exact bits our implementation gives, pinned so that
// any change to it, or any target that computes differently, shows up.

#include "richc/math/math.h"

#include "richc/test.h"

// An input and the bits of one of our functions' result for it.
typedef struct golden_f32 {
    uint32_t x;
    uint32_t result;
} golden_f32;

// Two inputs and the bits of pow's result for them.
typedef struct golden_pow_f32 {
    uint32_t x;
    uint32_t y;
    uint32_t result;
} golden_pow_f32;

// Each input is chosen for a branch or range of the code: tiny arguments, the table's steps, the
// edges of overflow and of the subnormals.
static const golden_f32 exp_golden[] = {
    {0x3f800000u, 0x402df854u},  // 1 -> 2.71828175
    {0xbf800000u, 0x3ebc5ab2u},  // -1 -> 0.36787945
    {0x3f000000u, 0x3fd3094cu},  // 0.5 -> 1.64872122
    {0xbe800000u, 0x3f475f7du},  // -0.25 -> 0.778800786
    {0x33800000u, 0x3f800001u},  // 5.96046448e-08 -> 1.00000012
    {0xb3800000u, 0x3f7fffffu},  // -5.96046448e-08 -> 0.99999994
    {0x2edbe6ffu, 0x3f800000u},  // 1.00000001e-10 -> 1
    {0x3a83126fu, 0x3f8020c9u},  // 0.00100000005 -> 1.00100052
    {0x3dcccccdu, 0x3f8d763eu},  // 0.100000001 -> 1.10517097
    {0xbdcccccdu, 0x3f67a36du},  // -0.100000001 -> 0.90483743
    {0x3f317218u, 0x40000000u},  // 0.693147182 -> 2
    {0x3fc00000u, 0x408f69ffu},  // 1.5 -> 4.48168898
    {0x40000000u, 0x40ec7326u},  // 2 -> 7.38905621
    {0x40490fdbu, 0x41b92025u},  // 3.14159274 -> 23.1406956
    {0x40a00000u, 0x431469c5u},  // 5 -> 148.413162
    {0x41200000u, 0x46ac14eeu},  // 10 -> 22026.4648
    {0xc1200000u, 0x383e6bceu},  // -10 -> 4.5399931e-05
    {0x41a00000u, 0x4de75844u},  // 20 -> 485165184
    {0xc1a00000u, 0x310da433u},  // -20 -> 2.06115369e-09
    {0x42000000u, 0x568fa1feu},  // 32 -> 7.8962957e+13
    {0x42a00000u, 0x792abbceu},  // 80 -> 5.54062248e+34
    {0x42b17217u, 0x7f7fff84u},  // 88.7228317 -> 3.40279852e+38
    {0xc2aeac4fu, 0x00800026u},  // -87.3365402 -> 1.17549968e-38
    {0xc2aeac50u, 0x007fffe6u},  // -87.3365479 -> 1.17549071e-38
    {0xc2b00000u, 0x0041edc4u},  // -88 -> 6.05460149e-39
    {0xc2c80000u, 0x0000001bu},  // -100 -> 3.78350585e-44
    {0xc2cff1b4u, 0x00000001u},  // -103.972076 -> 1.40129846e-45
    {0xc2ce8ed0u, 0x00000001u},  // -103.278931 -> 1.40129846e-45
    {0x3c23d70au, 0x3f814953u},  // 0.00999999978 -> 1.01005018
    {0xc0e00000u, 0x3a6f0b5du}   // -7 -> 0.000911881973
};

// Across the log table's intervals, both ends of the one centred on 1, either side of 1, the
// extremes, and subnormals.
static const golden_f32 log_golden[] = {
    {0x3f000000u, 0xbf317218u},  // 0.5 -> -0.693147182
    {0x40000000u, 0x3f317218u},  // 2 -> 0.693147182
    {0x40400000u, 0x3f8c9f54u},  // 3 -> 1.09861231
    {0x41200000u, 0x40135d8eu},  // 10 -> 2.30258512
    {0x3dcccccdu, 0xc0135d8eu},  // 0.100000001 -> -2.30258512
    {0x3a83126fu, 0xc0dd0c55u},  // 0.00100000005 -> -6.90775537
    {0x501502f9u, 0x41b834f1u},  // 1e+10 -> 23.0258503
    {0x3f800001u, 0x33ffffffu},  // 1.00000012 -> 1.19209282e-07
    {0x3f7fffffu, 0xb3800000u},  // 0.99999994 -> -5.96046448e-08
    {0x3f7d70a4u, 0xbc24aa20u},  // 0.99000001 -> -0.0100503266
    {0x3f8147aeu, 0x3c2306acu},  // 1.00999999 -> 0.00995032117
    {0x3fb33333u, 0x3eac4616u},  // 1.39999998 -> 0.336472213
    {0x3f333333u, 0xbeb69e1au},  // 0.699999988 -> -0.356674969
    {0x3f320000u, 0xbeba0ec4u},  // 0.6953125 -> -0.363393903
    {0x3fb1ffffu, 0x3ea8d569u},  // 1.39062488 -> 0.32975319
    {0x7f7fffffu, 0x42b17218u},  // 3.40282347e+38 -> 88.7228394
    {0x00800000u, 0xc2aeac50u},  // 1.17549435e-38 -> -87.3365479
    {0x00000001u, 0xc2ce8ed0u},  // 1.40129846e-45 -> -103.278931
    {0x00000003u, 0xc2cc5c53u},  // 4.20389539e-45 -> -102.180321
    {0x007fffffu, 0xc2aeac50u},  // 1.17549421e-38 -> -87.3365479
    {0x402df854u, 0x3f7fffffu},  // 2.71828175 -> 0.99999994
    {0x40490fdbu, 0x3f928683u},  // 3.14159274 -> 1.14472997
    {0x42c80000u, 0x40935d8eu},  // 100 -> 4.60517025
    {0x4640e6b7u, 0x4116bcabu},  // 12345.6787 -> 9.42106152
    {0x3e99999au, 0xbf9a1bc8u},  // 0.300000012 -> -1.20397282
    {0x40e00000u, 0x3ff91395u},  // 7 -> 1.9459101
    {0x0da24260u, 0xc28a27b5u},  // 1e-30 -> -69.0775528
    {0x7149f2cau, 0x428a27b5u},  // 1.00000002e+30 -> 69.0775528
    {0x3f7e0000u, 0xbc0080acu},  // 0.9921875 -> -0.00784317777
    {0x3f820000u, 0x3c7e0546u}   // 1.015625 -> 0.015504187
};

// As for log, plus the halfway points sqrt(2) and sqrt(1/2).
static const golden_f32 log2_golden[] = {
    {0x40400000u, 0x3fcae00du},  // 3 -> 1.58496249
    {0x41200000u, 0x40549a78u},  // 10 -> 3.32192802
    {0x3dcccccdu, 0xc0549a78u},  // 0.100000001 -> -3.32192802
    {0x3f800001u, 0x3438aa3au},  // 1.00000012 -> 1.71982634e-07
    {0x3f7fffffu, 0xb3b8aa3cu},  // 0.99999994 -> -8.59913314e-08
    {0x501502f9u, 0x4204e08bu},  // 1e+10 -> 33.2192802
    {0x7f7fffffu, 0x43000000u},  // 3.40282347e+38 -> 128
    {0x00000003u, 0xc3136a40u},  // 4.20389539e-45 -> -147.415039
    {0x007fffffu, 0xc2fc0000u},  // 1.17549421e-38 -> -126
    {0x00400001u, 0xc2fe0000u},  // 5.87747316e-39 -> -127
    {0x3fc00000u, 0x3f15c01au},  // 1.5 -> 0.584962487
    {0x3f400000u, 0xbed47fccu},  // 0.75 -> -0.415037513
    {0x402df854u, 0x3fb8aa3bu},  // 2.71828175 -> 1.44269502
    {0x40490fdbu, 0x3fd3643au},  // 3.14159274 -> 1.65149617
    {0x3f320000u, 0xbf063655u},  // 0.6953125 -> -0.524266541
    {0x3fb1ffffu, 0x3ef39351u},  // 1.39062488 -> 0.47573331
    {0x3f7d70a4u, 0xbc6d8f8bu},  // 0.99000001 -> -0.0144995553
    {0x3f8147aeu, 0x3c6b3268u},  // 1.00999999 -> 0.0143552795
    {0x42c80000u, 0x40d49a78u},  // 100 -> 6.64385605
    {0x4640e6b7u, 0x415977aeu},  // 12345.6787 -> 13.5917187
    {0x3e99999au, 0xbfde54e3u},  // 0.300000012 -> -1.73696554
    {0x40e00000u, 0x4033abb4u},  // 7 -> 2.80735493
    {0x0da24260u, 0xc2c750d1u},  // 1e-30 -> -99.6578445
    {0x7149f2cau, 0x42c750d1u},  // 1.00000002e+30 -> 99.6578445
    {0x3f7e0000u, 0xbc3963ddu},  // 0.9921875 -> -0.0113153132
    {0x3f820000u, 0x3cb73cb4u},  // 1.015625 -> 0.0223678127
    {0x3f3504f3u, 0xbf000000u},  // 0.707106769 -> -0.5
    {0x3fb504f3u, 0x3effffffu}   // 1.41421354 -> 0.49999997
};

// Square roots, cube roots, x near 1 with y large, overflow and underflow edges, negative x, and
// subnormal x and results.
static const golden_pow_f32 pow_golden[] = {
    {0x40000000u, 0x3f000000u, 0x3fb504f3u},  // 2 ^ 0.5 -> 1.41421354
    {0x40000000u, 0x41200000u, 0x44800000u},  // 2 ^ 10 -> 1024
    {0x41200000u, 0xc0000000u, 0x3c23d70au},  // 10 ^ -2 -> 0.00999999978
    {0x3f000000u, 0x42c80000u, 0x0d800000u},  // 0.5 ^ 100 -> 7.88860905e-31
    {0x40400000u, 0x3eaaaaabu, 0x3fb89ba2u},  // 3 ^ 0.333333343 -> 1.44224954
    {0x3f800347u, 0x461c4000u, 0x402dfd7eu},  // 1.00010002 ^ 10000 -> 2.71859694
    {0x3f7ff972u, 0xc61c4000u, 0x402e01f3u},  // 0.999899983 ^ -10000 -> 2.71886897
    {0x40200000u, 0x41f00000u, 0x5349f2cau},  // 2.5 ^ 30 -> 8.67361751e+11
    {0x42c80000u, 0x4199999au, 0x7f3cf958u},  // 100 ^ 19.2000008 -> 2.5118953e+38
    {0x42c80000u, 0xc1b00000u, 0x00000007u},  // 100 ^ -22 -> 9.80908925e-45
    {0x40000000u, 0xc3150000u, 0x00000001u},  // 2 ^ -149 -> 1.40129846e-45
    {0x40000000u, 0xc3158000u, 0x00000001u},  // 2 ^ -149.5 -> 1.40129846e-45
    {0xc0000000u, 0x40400000u, 0xc1000000u},  // (-2) ^ 3 -> -8
    {0xc0000000u, 0xc0400000u, 0xbe000000u},  // (-2) ^ -3 -> -0.125
    {0xbf800001u, 0x4b7fffffu, 0xc0ec7322u},  // (-1.00000012) ^ 16777215 -> -7.3890543
    {0xbf000000u, 0x41c00000u, 0x33800000u},  // (-0.5) ^ 24 -> 5.96046448e-08
    {0xbfc00000u, 0x4b000001u, 0xff800000u},  // (-1.5) ^ 8388609 -> -inf
    {0x00000200u, 0x3f000000u, 0x1c800000u},  // 7.17464814e-43 ^ 0.5 -> 8.47032947e-22
    {0x1e3ce508u, 0x3ff33333u, 0x006ce3f6u},  // 9.99999968e-21 ^ 1.89999998 -> 1.00000106e-38
    {0x40e00000u, 0x3f8ccccdu, 0x41080f26u},  // 7 ^ 1.10000002 -> 8.50369835
    {0x3e99999au, 0x3f333333u, 0x3edc6c05u},  // 0.300000012 ^ 0.699999988 -> 0.430511624
    {0x3fc00000u, 0xc2480000u, 0x30d78ca8u},  // 1.5 ^ -50 -> 1.56832858e-09
    {0x40a00000u, 0x425c0000u, 0x7f50cf4bu},  // 5 ^ 55 -> 2.7755575e+38
    {0x3dcccccdu, 0x42340000u, 0x00000001u},  // 0.100000001 ^ 45 -> 1.40129846e-45
    {0xbdcccccdu, 0x42340000u, 0x80000001u},  // (-0.100000001) ^ 45 -> -1.40129846e-45
    {0x41480000u, 0xc20e0000u, 0x000c7e56u},  // 12.5 ^ -35.5 -> 1.14734675e-39
    {0x3f800001u, 0xcb000000u, 0x3ebc5ab2u},  // 1.00000012 ^ -8388608 -> 0.36787945
    {0x3f7fffffu, 0x4eee6b28u, 0x00000000u},  // 0.99999994 ^ 2e+09 -> 0
    {0x41100000u, 0x3fc00000u, 0x41d80000u},  // 9 ^ 1.5 -> 27
    {0x40800000u, 0xbf000000u, 0x3f000000u},  // 4 ^ -0.5 -> 0.5
    {0xc1000000u, 0x40000000u, 0x42800000u},  // (-8) ^ 2 -> 64
    {0x40400000u, 0x42a10000u, 0x7f409a32u}   // 3 ^ 80.5 -> 2.56012403e+38
};


// True for a NaN, of any sign or payload.
static bool is_nan(float x)
{
    return !rc_is_finite_f32(x) && x != x;
}


// The bits of e^x.
static uint32_t exp_bits(float x)
{
    return rc_bitcast_f32(rc_exp_f32(x));
}


// The bits of log(x).
static uint32_t log_bits(float x)
{
    return rc_bitcast_f32(rc_log_f32(x));
}


// The bits of log2(x).
static uint32_t log2_bits(float x)
{
    return rc_bitcast_f32(rc_log2_f32(x));
}


// The bits of x^y.
static uint32_t pow_bits(float x, float y)
{
    return rc_bitcast_f32(rc_pow_f32(x, y));
}


// ---- exp ----

RC_TEST(math_exp_f32, special_values)
{
    float infinity = rc_bitcast_u32(0x7f800000u);
    float nan = rc_bitcast_u32(0x7fc00000u);

    RC_CHECK(exp_bits(0.0f), ==, 0x3f800000u);
    RC_CHECK(exp_bits(-0.0f), ==, 0x3f800000u);
    RC_CHECK(exp_bits(infinity), ==, 0x7f800000u);
    RC_CHECK(exp_bits(-infinity), ==, 0x00000000u);
    RC_CHECK_TRUE(is_nan(rc_exp_f32(nan)));
    RC_CHECK_TRUE(is_nan(rc_exp_f32(-nan)));
}


RC_TEST(math_exp_f32, overflow_and_underflow)
{
    // 88.7228317 is the last x whose e^x is finite, and the float after it the first that
    // overflows, where the main path hands over to the slow one
    RC_CHECK(exp_bits(rc_bitcast_u32(0x42b17217u)), ==, 0x7f7fff84u);
    RC_CHECK(exp_bits(rc_bitcast_u32(0x42b17218u)), ==, 0x7f800000u);
    RC_CHECK(exp_bits(100.0f), ==, 0x7f800000u);
    RC_CHECK(exp_bits(104.0f), ==, 0x7f800000u);
    RC_CHECK(exp_bits(rc_bitcast_u32(0x7f7fffffu)), ==, 0x7f800000u);

    // Either side of 600, far beyond the main path
    RC_CHECK(exp_bits(rc_bitcast_u32(0x4415ffffu)), ==, 0x7f800000u);
    RC_CHECK(exp_bits(600.0f), ==, 0x7f800000u);
    RC_CHECK(exp_bits(rc_bitcast_u32(0xc415ffffu)), ==, 0x00000000u);
    RC_CHECK(exp_bits(-600.0f), ==, 0x00000000u);

    // -103.972076 gives the smallest subnormal, and the float below it rounds to +0, where the main
    // path hands over to the slow one
    RC_CHECK(exp_bits(rc_bitcast_u32(0xc2cff1b4u)), ==, 0x00000001u);
    RC_CHECK(exp_bits(rc_bitcast_u32(0xc2cff1b5u)), ==, 0x00000000u);
    RC_CHECK(exp_bits(-104.0f), ==, 0x00000000u);
    RC_CHECK(exp_bits(-1000.0f), ==, 0x00000000u);
    RC_CHECK(exp_bits(rc_bitcast_u32(0xff7fffffu)), ==, 0x00000000u);
}


RC_TEST(math_exp_f32, subnormal_results)
{
    // e^-87.3365479 is just below the smallest normal, and e^-100 deep among the subnormals
    RC_CHECK(exp_bits(rc_bitcast_u32(0xc2aeac4fu)), ==, 0x00800026u);
    RC_CHECK(exp_bits(rc_bitcast_u32(0xc2aeac50u)), ==, 0x007fffe6u);
    RC_CHECK(exp_bits(-100.0f), ==, 0x0000001bu);
}


RC_TEST(math_exp_f32, readable)
{
    // e, 1/e and 2 correctly rounded
    RC_CHECK(rc_exp_f32(1.0f), ==, 2.71828183f);
    RC_CHECK(rc_exp_f32(-1.0f), ==, 0.36787944f);
    RC_CHECK(rc_exp_f32(0.69314718f), ==, 2.0f);

    // Tiny arguments land on 1, or the float either side of it
    RC_CHECK(rc_exp_f32(1e-10f), ==, 1.0f);
    RC_CHECK(exp_bits(0x1p-24f), ==, 0x3f800001u);
    RC_CHECK(exp_bits(-0x1p-24f), ==, 0x3f7fffffu);
}


RC_TEST(math_exp_f32, golden)
{
    for (uint32_t i = 0; i < RC_COUNT(exp_golden); i++) {
        RC_CHECK(exp_bits(rc_bitcast_u32(exp_golden[i].x)), ==, exp_golden[i].result);
    }
}


// ---- log ----

RC_TEST(math_log_f32, special_values)
{
    float infinity = rc_bitcast_u32(0x7f800000u);
    float nan = rc_bitcast_u32(0x7fc00000u);

    RC_CHECK(log_bits(0.0f), ==, 0xff800000u);
    RC_CHECK(log_bits(-0.0f), ==, 0xff800000u);
    RC_CHECK(log_bits(1.0f), ==, 0x00000000u);
    RC_CHECK(log_bits(infinity), ==, 0x7f800000u);
    RC_CHECK_TRUE(is_nan(rc_log_f32(-1.0f)));
    RC_CHECK_TRUE(is_nan(rc_log_f32(-infinity)));
    RC_CHECK_TRUE(is_nan(rc_log_f32(rc_bitcast_u32(0xff7fffffu))));
    RC_CHECK_TRUE(is_nan(rc_log_f32(rc_bitcast_u32(0x80000001u))));
    RC_CHECK_TRUE(is_nan(rc_log_f32(nan)));
    RC_CHECK_TRUE(is_nan(rc_log_f32(-nan)));
}


RC_TEST(math_log_f32, readable)
{
    RC_CHECK(rc_log_f32(2.0f), ==, 0.69314718f);
    RC_CHECK(rc_log_f32(0.5f), ==, -0.69314718f);
    RC_CHECK(rc_log_f32(2.71828183f), ~=, 1.0f);
    RC_CHECK(rc_log_f32(10.0f), ==, 2.30258509f);

    // The floats either side of 1
    RC_CHECK(log_bits(rc_bitcast_u32(0x3f800001u)), ==, 0x33ffffffu);
    RC_CHECK(log_bits(rc_bitcast_u32(0x3f7fffffu)), ==, 0xb3800000u);

    // The smallest subnormal and the largest float
    RC_CHECK(rc_log_f32(rc_bitcast_u32(0x00000001u)), ==, -103.278929f);
    RC_CHECK(rc_log_f32(rc_bitcast_u32(0x7f7fffffu)), ==, 88.7228391f);
}


RC_TEST(math_log_f32, golden)
{
    for (uint32_t i = 0; i < RC_COUNT(log_golden); i++) {
        RC_CHECK(log_bits(rc_bitcast_u32(log_golden[i].x)), ==, log_golden[i].result);
    }
}


// ---- log2 ----

RC_TEST(math_log2_f32, special_values)
{
    float infinity = rc_bitcast_u32(0x7f800000u);
    float nan = rc_bitcast_u32(0x7fc00000u);

    RC_CHECK(log2_bits(0.0f), ==, 0xff800000u);
    RC_CHECK(log2_bits(-0.0f), ==, 0xff800000u);
    RC_CHECK(log2_bits(1.0f), ==, 0x00000000u);
    RC_CHECK(log2_bits(infinity), ==, 0x7f800000u);
    RC_CHECK_TRUE(is_nan(rc_log2_f32(-1.0f)));
    RC_CHECK_TRUE(is_nan(rc_log2_f32(-infinity)));
    RC_CHECK_TRUE(is_nan(rc_log2_f32(rc_bitcast_u32(0xff7fffffu))));
    RC_CHECK_TRUE(is_nan(rc_log2_f32(rc_bitcast_u32(0x80000001u))));
    RC_CHECK_TRUE(is_nan(rc_log2_f32(nan)));
    RC_CHECK_TRUE(is_nan(rc_log2_f32(-nan)));
}


RC_TEST(math_log2_f32, every_power_of_2)
{
    // 2^-149 to 2^-127 are subnormals, a single bit each
    for (int32_t k = -149; k < -126; k++) {
        RC_CHECK(rc_log2_f32(rc_bitcast_u32(1u << (k + 149))), ==, (float)k);
    }

    for (int32_t k = -126; k <= 127; k++) {
        RC_CHECK(rc_log2_f32(rc_bitcast_u32((uint32_t)(k + 127) << 23)), ==, (float)k);
    }
}


RC_TEST(math_log2_f32, readable)
{
    RC_CHECK(rc_log2_f32(8.0f), ==, 3.0f);
    RC_CHECK(rc_log2_f32(0.125f), ==, -3.0f);
    RC_CHECK(rc_log2_f32(1024.0f), ==, 10.0f);
    RC_CHECK(rc_log2_f32(10.0f), ==, 3.32192809f);
    RC_CHECK(rc_log2_f32(3.0f), ==, 1.58496250f);

    // The largest float is just short of 2^128, and its log2 rounds up to 128
    RC_CHECK(rc_log2_f32(rc_bitcast_u32(0x7f7fffffu)), ==, 128.0f);
}


RC_TEST(math_log2_f32, golden)
{
    for (uint32_t i = 0; i < RC_COUNT(log2_golden); i++) {
        RC_CHECK(log2_bits(rc_bitcast_u32(log2_golden[i].x)), ==, log2_golden[i].result);
    }
}


// ---- pow: Annex F ----

RC_TEST(math_pow_f32, y_zero_gives_1)
{
    float infinity = rc_bitcast_u32(0x7f800000u);
    float nan = rc_bitcast_u32(0x7fc00000u);
    float smallest = rc_bitcast_u32(0x00000001u);
    float largest = rc_bitcast_u32(0x7f7fffffu);
    float xs[] = {0.0f, -0.0f, 1.0f, -1.0f, 2.5f, -2.5f, smallest, -smallest, largest, -largest, infinity, -infinity, nan};

    for (uint32_t i = 0; i < RC_COUNT(xs); i++) {
        RC_CHECK(pow_bits(xs[i], 0.0f), ==, 0x3f800000u);
        RC_CHECK(pow_bits(xs[i], -0.0f), ==, 0x3f800000u);
    }
}


RC_TEST(math_pow_f32, x_one_gives_1)
{
    float infinity = rc_bitcast_u32(0x7f800000u);
    float nan = rc_bitcast_u32(0x7fc00000u);
    float ys[] = {0.0f, -0.0f, 2.0f, -2.0f, 0.5f, rc_bitcast_u32(0x00000001u), rc_bitcast_u32(0x7f7fffffu), infinity, -infinity, nan};

    for (uint32_t i = 0; i < RC_COUNT(ys); i++) {
        RC_CHECK(pow_bits(1.0f, ys[i]), ==, 0x3f800000u);
    }

    RC_CHECK(pow_bits(-1.0f, infinity), ==, 0x3f800000u);
    RC_CHECK(pow_bits(-1.0f, -infinity), ==, 0x3f800000u);
}


RC_TEST(math_pow_f32, zero_x)
{
    float infinity = rc_bitcast_u32(0x7f800000u);
    float negative_odd[] = {-1.0f, -3.0f, -16777215.0f};
    float negative_other[] = {-2.0f, -0.5f, -1.5f, -16777216.0f, rc_bitcast_u32(0xff7fffffu), rc_bitcast_u32(0x80000001u)};
    float positive_odd[] = {1.0f, 3.0f, 16777215.0f};
    float positive_other[] = {2.0f, 0.5f, 16777216.0f, rc_bitcast_u32(0x7f7fffffu), rc_bitcast_u32(0x00000001u)};

    // A negative odd y keeps x's sign on the infinity; any other negative y gives +inf
    for (uint32_t i = 0; i < RC_COUNT(negative_odd); i++) {
        RC_CHECK(pow_bits(0.0f, negative_odd[i]), ==, 0x7f800000u);
        RC_CHECK(pow_bits(-0.0f, negative_odd[i]), ==, 0xff800000u);
    }

    for (uint32_t i = 0; i < RC_COUNT(negative_other); i++) {
        RC_CHECK(pow_bits(0.0f, negative_other[i]), ==, 0x7f800000u);
        RC_CHECK(pow_bits(-0.0f, negative_other[i]), ==, 0x7f800000u);
    }

    // A positive odd y keeps x's sign on the zero; any other positive y gives +0
    for (uint32_t i = 0; i < RC_COUNT(positive_odd); i++) {
        RC_CHECK(pow_bits(0.0f, positive_odd[i]), ==, 0x00000000u);
        RC_CHECK(pow_bits(-0.0f, positive_odd[i]), ==, 0x80000000u);
    }

    for (uint32_t i = 0; i < RC_COUNT(positive_other); i++) {
        RC_CHECK(pow_bits(0.0f, positive_other[i]), ==, 0x00000000u);
        RC_CHECK(pow_bits(-0.0f, positive_other[i]), ==, 0x00000000u);
    }

    RC_CHECK(pow_bits(0.0f, -infinity), ==, 0x7f800000u);
    RC_CHECK(pow_bits(-0.0f, -infinity), ==, 0x7f800000u);
    RC_CHECK(pow_bits(0.0f, infinity), ==, 0x00000000u);
    RC_CHECK(pow_bits(-0.0f, infinity), ==, 0x00000000u);
}


RC_TEST(math_pow_f32, infinite_y)
{
    float infinity = rc_bitcast_u32(0x7f800000u);
    float below_1[] = {0.5f, -0.5f, rc_bitcast_u32(0x3f7fffffu), rc_bitcast_u32(0xbf7fffffu), rc_bitcast_u32(0x00000001u)};
    float largest = rc_bitcast_u32(0x7f7fffffu);
    float above_1[] = {2.0f, -2.0f, rc_bitcast_u32(0x3f800001u), rc_bitcast_u32(0xbf800001u), largest, -largest, infinity, -infinity};

    for (uint32_t i = 0; i < RC_COUNT(below_1); i++) {
        RC_CHECK(pow_bits(below_1[i], -infinity), ==, 0x7f800000u);
        RC_CHECK(pow_bits(below_1[i], infinity), ==, 0x00000000u);
    }

    for (uint32_t i = 0; i < RC_COUNT(above_1); i++) {
        RC_CHECK(pow_bits(above_1[i], -infinity), ==, 0x00000000u);
        RC_CHECK(pow_bits(above_1[i], infinity), ==, 0x7f800000u);
    }
}


RC_TEST(math_pow_f32, infinite_x)
{
    float infinity = rc_bitcast_u32(0x7f800000u);
    float negative_odd[] = {-1.0f, -3.0f, -16777215.0f};
    float negative_other[] = {-2.0f, -0.5f, -16777216.0f, rc_bitcast_u32(0xff7fffffu), rc_bitcast_u32(0x80000001u)};
    float positive_odd[] = {1.0f, 3.0f, 16777215.0f};
    float positive_other[] = {2.0f, 0.5f, 16777216.0f, rc_bitcast_u32(0x7f7fffffu), rc_bitcast_u32(0x00000001u)};

    // -inf follows the same pattern as -0, the other way up
    for (uint32_t i = 0; i < RC_COUNT(negative_odd); i++) {
        RC_CHECK(pow_bits(-infinity, negative_odd[i]), ==, 0x80000000u);
        RC_CHECK(pow_bits(infinity, negative_odd[i]), ==, 0x00000000u);
    }

    for (uint32_t i = 0; i < RC_COUNT(negative_other); i++) {
        RC_CHECK(pow_bits(-infinity, negative_other[i]), ==, 0x00000000u);
        RC_CHECK(pow_bits(infinity, negative_other[i]), ==, 0x00000000u);
    }

    for (uint32_t i = 0; i < RC_COUNT(positive_odd); i++) {
        RC_CHECK(pow_bits(-infinity, positive_odd[i]), ==, 0xff800000u);
        RC_CHECK(pow_bits(infinity, positive_odd[i]), ==, 0x7f800000u);
    }

    for (uint32_t i = 0; i < RC_COUNT(positive_other); i++) {
        RC_CHECK(pow_bits(-infinity, positive_other[i]), ==, 0x7f800000u);
        RC_CHECK(pow_bits(infinity, positive_other[i]), ==, 0x7f800000u);
    }
}


RC_TEST(math_pow_f32, nan)
{
    float infinity = rc_bitcast_u32(0x7f800000u);
    float nan = rc_bitcast_u32(0x7fc00000u);
    float others[] = {0.0f, -0.0f, -1.0f, 2.0f, -2.0f, 0.5f, rc_bitcast_u32(0x00000001u), infinity, -infinity, nan};

    // NaN in either argument gives NaN, but for the y = 0 and x = 1 rows above
    for (uint32_t i = 0; i < RC_COUNT(others); i++) {
        RC_CHECK_TRUE(is_nan(rc_pow_f32(nan, others[i] == 0.0f ? 1.0f : others[i])));
        RC_CHECK_TRUE(is_nan(rc_pow_f32(others[i], nan)));
    }

    RC_CHECK_TRUE(is_nan(rc_pow_f32(-nan, 2.0f)));
    RC_CHECK_TRUE(is_nan(rc_pow_f32(2.0f, -nan)));
}


// ---- pow: negative x ----

RC_TEST(math_pow_f32, negative_x_needs_a_whole_y)
{
    float non_integers[] = {0.5f, -0.5f, 1.5f, rc_bitcast_u32(0x3f800001u), 8388607.5f, -8388607.5f, rc_bitcast_u32(0x00000001u), 1e-20f};
    float xs[] = {-1.0f, -2.0f, -0.5f, rc_bitcast_u32(0xff7fffffu), rc_bitcast_u32(0x80000001u)};

    for (uint32_t i = 0; i < RC_COUNT(xs); i++) {
        for (uint32_t j = 0; j < RC_COUNT(non_integers); j++) {
            RC_CHECK_TRUE(is_nan(rc_pow_f32(xs[i], non_integers[j])));
        }
    }
}


RC_TEST(math_pow_f32, negative_x_odd_and_even_y)
{
    // -1 to an odd power is -1 and to an even one 1, right up to where the floats are all even
    RC_CHECK(rc_pow_f32(-1.0f, 3.0f), ==, -1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, -3.0f), ==, -1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, 4.0f), ==, 1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, 8388607.0f), ==, -1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, 8388608.0f), ==, 1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, 8388609.0f), ==, -1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, -8388609.0f), ==, -1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, 16777215.0f), ==, -1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, 16777216.0f), ==, 1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, 16777218.0f), ==, 1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, -16777218.0f), ==, 1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, 1e30f), ==, 1.0f);
    RC_CHECK(rc_pow_f32(-1.0f, rc_bitcast_u32(0x7f7fffffu)), ==, 1.0f);

    // Exact results keep their sign
    RC_CHECK(rc_pow_f32(-2.0f, 3.0f), ==, -8.0f);
    RC_CHECK(rc_pow_f32(-2.0f, -3.0f), ==, -0.125f);
    RC_CHECK(rc_pow_f32(-3.0f, 4.0f), ==, 81.0f);
    RC_CHECK(rc_pow_f32(-0.5f, 2.0f), ==, 0.25f);
    RC_CHECK(rc_pow_f32(-7.0f, 1.0f), ==, -7.0f);

    // Just off -1, near 2^23 and 2^24: (1 + 2^-23)^(2^23 + 1) is about e
    RC_CHECK(pow_bits(rc_bitcast_u32(0xbf800001u), 8388609.0f), ==, 0xc02df855u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0xbf800001u), 16777215.0f), ==, 0xc0ec7322u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0xbf800001u), 16777216.0f), ==, 0x40ec7324u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0xbf7fffffu), 16777218.0f), ==, 0x3ebc5ab0u);
}


// ---- pow: overflow, underflow and the subnormals ----

RC_TEST(math_pow_f32, overflow)
{
    RC_CHECK(pow_bits(10.0f, 39.0f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(-10.0f, 39.0f), ==, 0xff800000u);
    RC_CHECK(pow_bits(-10.0f, 40.0f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(2.0f, 128.0f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(-2.0f, 129.0f), ==, 0xff800000u);
    RC_CHECK(pow_bits(0.5f, -128.0f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x7f7fffffu), 2.0f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x00000001u), -1.0f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x3f800001u), 1e10f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0xbf800001u), 1e10f), ==, 0x7f800000u);

    // Far enough out that the main path gives up on the kernel, keeping the sign of an odd power
    RC_CHECK(pow_bits(2.0f, 1001.0f), ==, 0x7f800000u);
    RC_CHECK(pow_bits(-2.0f, 1001.0f), ==, 0xff800000u);
    RC_CHECK(pow_bits(-2.0f, 1002.0f), ==, 0x7f800000u);

    // The largest exact power of 2 is still finite
    RC_CHECK(pow_bits(2.0f, 127.0f), ==, 0x7f000000u);
    RC_CHECK(pow_bits(-2.0f, 127.0f), ==, 0xff000000u);
}


RC_TEST(math_pow_f32, underflow)
{
    RC_CHECK(pow_bits(10.0f, -47.0f), ==, 0x00000000u);
    RC_CHECK(pow_bits(-10.0f, -47.0f), ==, 0x80000000u);
    RC_CHECK(pow_bits(-10.0f, -48.0f), ==, 0x00000000u);
    RC_CHECK(pow_bits(0.5f, 151.0f), ==, 0x00000000u);
    RC_CHECK(pow_bits(-0.5f, 151.0f), ==, 0x80000000u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x3f7fffffu), 1e10f), ==, 0x00000000u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x7f7fffffu), -2.0f), ==, 0x00000000u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x80000001u), 2.0f), ==, 0x00000000u);
    RC_CHECK(pow_bits(2.0f, -1001.0f), ==, 0x00000000u);
    RC_CHECK(pow_bits(-2.0f, -1001.0f), ==, 0x80000000u);
    RC_CHECK(pow_bits(-2.0f, -1002.0f), ==, 0x00000000u);

    // 2^-150 is halfway between +0 and the smallest subnormal, and rounds to the even +0
    RC_CHECK(pow_bits(2.0f, -150.0f), ==, 0x00000000u);
    RC_CHECK(pow_bits(-2.0f, -151.0f), ==, 0x80000000u);
}


RC_TEST(math_pow_f32, subnormal_results)
{
    RC_CHECK(pow_bits(2.0f, -149.0f), ==, 0x00000001u);
    RC_CHECK(pow_bits(-2.0f, -149.0f), ==, 0x80000001u);
    RC_CHECK(pow_bits(2.0f, -140.0f), ==, 0x00000200u);
    RC_CHECK(pow_bits(0.5f, 130.0f), ==, 0x00080000u);
    RC_CHECK(pow_bits(-2.0f, -147.0f), ==, 0x80000004u);
    RC_CHECK(pow_bits(10.0f, -40.0f), ==, 0x000116c2u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x3f800001u), -8.4e8f), ==, 0x00000017u);
}


RC_TEST(math_pow_f32, subnormal_x)
{
    // 2^-140 and 2^-149 are subnormals, with normal roots; -2^-140 cubed underflows, and its
    // reciprocal overflows
    RC_CHECK(pow_bits(rc_bitcast_u32(0x00000200u), 0.5f), ==, 0x1c800000u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x00000200u), -0.5f), ==, 0x62800000u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x00000001u), 0.5f), ==, 0x1a3504f3u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x80000200u), 3.0f), ==, 0x80000000u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x80000200u), -1.0f), ==, 0xff800000u);

    // x^1 is x, sign and all
    RC_CHECK(pow_bits(rc_bitcast_u32(0x00000001u), 1.0f), ==, 0x00000001u);
    RC_CHECK(pow_bits(rc_bitcast_u32(0x807fffffu), 1.0f), ==, 0x807fffffu);
}


RC_TEST(math_pow_f32, exact_results)
{
    RC_CHECK(rc_pow_f32(2.0f, 10.0f), ==, 1024.0f);
    RC_CHECK(rc_pow_f32(3.0f, 2.0f), ==, 9.0f);
    RC_CHECK(rc_pow_f32(10.0f, 3.0f), ==, 1000.0f);
    RC_CHECK(rc_pow_f32(9.0f, 0.5f), ==, 3.0f);
    RC_CHECK(rc_pow_f32(16.0f, 0.25f), ==, 2.0f);
    RC_CHECK(rc_pow_f32(27.0f, -1.0f / 3.0f), ~=, 1.0f / 3.0f);
    RC_CHECK(rc_pow_f32(9.0f, 1.5f), ==, 27.0f);
    RC_CHECK(rc_pow_f32(4.0f, -0.5f), ==, 0.5f);
    RC_CHECK(rc_pow_f32(2.0f, -3.0f), ==, 0.125f);
    RC_CHECK(rc_pow_f32(1.5f, 1.0f), ==, 1.5f);
    RC_CHECK(rc_pow_f32(12345.678f, 1.0f), ==, 12345.678f);
}


RC_TEST(math_pow_f32, readable)
{
    RC_CHECK(rc_pow_f32(2.0f, 0.5f), ==, 1.41421356f);
    RC_CHECK(rc_pow_f32(10.0f, -2.0f), ==, 0.01f);
    RC_CHECK(rc_pow_f32(2.71828183f, 2.0f), ~=, 7.3890561f);
    RC_CHECK(rc_pow_f32(1.0001f, 10000.0f), ~=, 2.7186f);
}


RC_TEST(math_pow_f32, golden)
{
    for (uint32_t i = 0; i < RC_COUNT(pow_golden); i++) {
        RC_CHECK(pow_bits(rc_bitcast_u32(pow_golden[i].x), rc_bitcast_u32(pow_golden[i].y)), ==, pow_golden[i].result);
    }
}
