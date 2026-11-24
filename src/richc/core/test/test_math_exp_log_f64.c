// test_math_exp_log_f64.c
//
// Tests for rc_exp_f64, rc_log_f64, rc_log2_f64 and rc_pow_f64: every special value C gives them,
// checked bit for bit, some readable results, and golden bits for inputs across every branch and
// range of each, which pin the results so that any change, or a target that computes differently,
// shows up. How accurate they are is richc_math_accuracy's business.

#include "richc/math/math.h"

#include "richc/test.h"

// Inputs and the bits rc_exp_f64 gives for them: tiny, around the table's steps, ordinary, either
// side of the fast path's edge, near overflow, and down through the subnormals.
static const uint64_t exp_golden[][2] = {
    {0x0000000000000001ull, 0x3ff0000000000000ull},  // 4.94065646e-324 -> 1
    {0x01a56e1fc2f8f359ull, 0x3ff0000000000000ull},  // 1e-300 -> 1
    {0xbbc79ca10c924223ull, 0x3ff0000000000000ull},  // -1e-20 -> 1
    {0x3e10000000000000ull, 0x3ff0000000400000ull},  // 9.31322575e-10 -> 1
    {0x3ee4f8b588e368f1ull, 0x3ff0000a7c5e340eull},  // 1e-05 -> 1.00001
    {0xbf50624dd2f1a9fcull, 0x3feff7cfe56f1a9eull},  // -0.001 -> 0.9990005
    {0x3f661e4f765fd8aeull, 0x3ff00b12fb0f4433ull},  // 0.0027 -> 1.00270365
    {0x3fe0000000000000ull, 0x3ffa61298e1e069cull},  // 0.5 -> 1.64872127
    {0xbfe0000000000000ull, 0x3fe368b2fc6f960aull},  // -0.5 -> 0.60653066
    {0x3ff0000000000000ull, 0x4005bf0a8b145769ull},  // 1 -> 2.71828183
    {0xbff0000000000000ull, 0x3fd78b56362cef38ull},  // -1 -> 0.367879441
    {0x3fe62e42fefa39efull, 0x4000000000000000ull},  // 0.693147181 -> 2
    {0x4004000000000000ull, 0x40285d6fd931e0bbull},  // 2.5 -> 12.182494
    {0xc00d99999999999aull, 0x3f99511fc6871044ull},  // -3.7 -> 0.0247235265
    {0x4024000000000000ull, 0x40d5829dcf950560ull},  // 10 -> 22026.4658
    {0xc024000000000000ull, 0x3f07cd79b5647c9bull},  // -10 -> 4.53999298e-05
    {0x4042c00000000000ull, 0x4351293919c201e5ull},  // 37.5 -> 1.93215993e+16
    {0x4059000000000000ull, 0x48f3494a9b171bf5ull},  // 100 -> 2.68811714e+43
    {0xc059000000000000ull, 0x36ea8c1f14e2af5dull},  // -100 -> 3.72007598e-44
    {0x4072c40000000000ull, 0x5b01fda3eaeb3d0eull},  // 300.25 -> 2.49412486e+130
    {0xc07f4c0000000000ull, 0x12c7c28a55a0f633ull},  // -500.75 -> 3.3654116e-218
    {0x4085de0000000000ull, 0x7f07095d888378aeull},  // 699.75 -> 7.89884718e+303
    {0xc085de0000000000ull, 0x00d639bb8c099236ull},  // -699.75 -> 1.26600753e-304
    {0x4085e00000000000ull, 0x7f0d945df4f8ec8eull},  // 700 -> 1.01423205e+304
    {0xc085e00000000000ull, 0x00d14f2b0fb9307full},  // -700 -> 9.85967654e-305
    {0x4086040000000000ull, 0x7f74cd5583300ad2ull},  // 704.5 -> 9.129826e+305
    {0xc086040000000000ull, 0x00689ce8128f015bull},  // -704.5 -> 1.09531113e-306
    {0x4086240000000000ull, 0x7fd1bf058bc994adull},  // 708.5 -> 4.9847161e+307
    {0x4086280000000000ull, 0x7fdd422d2be5dc9bull},  // 709 -> 8.21840746e+307
    {0x40862e3d70a3d70aull, 0x7fefe9ce5c4c52b4ull},  // 709.78 -> 1.79282279e+308
    {0x40862e42fefa39efull, 0x7fefffffffffff2aull},  // 709.782713 -> 1.79769313e+308
    {0xc086200000000000ull, 0x0017c8ab2288c9abull},  // -708 -> 3.307553e-308
    {0xc086240000000000ull, 0x000e6cf6d08897acull},  // -708.5 -> 2.00613231e-308
    {0xc086800000000000ull, 0x0000000993b4dc95ull},  // -720 -> 2.0322308e-313
    {0xc087200000000000ull, 0x0000000000000055ull},  // -740 -> 4.19955799e-322
    {0xc087480000000000ull, 0x0000000000000001ull},  // -745 -> 4.94065646e-324
    {0xc08748cccccccccdull, 0x0000000000000001ull},  // -745.1 -> 4.94065646e-324
};

// Inputs and the bits rc_log_f64 gives for them: subnormal, either side of 1, at the edges of the
// tables' intervals and of log2's careful range [1/2, 2), ordinary and huge.
static const uint64_t log_golden[][2] = {
    {0x0000000000000001ull, 0xc0874385446d71c3ull},  // 4.94065646e-324 -> -744.440072
    {0x0008000000000000ull, 0xc08628b76e3a7b61ull},  // 1.11253693e-308 -> -709.089566
    {0x0010000000000000ull, 0xc086232bdd7abcd2ull},  // 2.22507386e-308 -> -708.396419
    {0x01a56e1fc2f8f359ull, 0xc085963447f87fb5ull},  // 1e-300 -> -690.775528
    {0x3ddb7cdfd9d7bdbbull, 0xc037069e2aa2aa5bull},  // 1e-10 -> -23.0258509
    {0x3fb999999999999aull, 0xc0026bb1bbb55515ull},  // 0.1 -> -2.30258509
    {0x3fdfffffffffffffull, 0xbfe62e42fefa39f0ull},  // 0.5 -> -0.693147181
    {0x3fe0000000000000ull, 0xbfe62e42fefa39efull},  // 0.5 -> -0.693147181
    {0x3fe6a09e667f3bcdull, 0xbfd62e42fefa39eeull},  // 0.707106781 -> -0.34657359
    {0x3feccccccccccccdull, 0xbfbaf8e8210a415cull},  // 0.9 -> -0.105360516
    {0x3fefae147ae147aeull, 0xbf8495453e6fd4bcull},  // 0.99 -> -0.0100503359
    {0x3fefffffffffffffull, 0xbca0000000000000ull},  // 1 -> -1.11022302e-16
    {0x3feffffe00000000ull, 0xbeb0000080000555ull},  // 0.999999046 -> -9.53674771e-07
    {0x3ff0000000000001ull, 0x3cafffffffffffffull},  // 1 -> 2.22044605e-16
    {0x3ff0000100000000ull, 0x3eafffff00000aabull},  // 1.00000095 -> 9.53673862e-07
    {0x3ff0100000000000ull, 0x3f6ff00aa2b10bc0ull},  // 1.00390625 -> 0.00389864042
    {0x3ff00ff972474539ull, 0x3f6fe2fc3cf4b407ull},  // 1.0039 -> 0.00389241472
    {0x3ff0200000000000ull, 0x3f7fe02a6b106789ull},  // 1.0078125 -> 0.00778214044
    {0x3ff01f8a0902de01ull, 0x3f7f6b1bdb86b310ull},  // 1.0077 -> 0.0076705063
    {0x3ff8000000000000ull, 0x3fd9f323ecbf984cull},  // 1.5 -> 0.405465108
    {0x3fffe00000000000ull, 0x3fe60e32f44788d9ull},  // 1.9921875 -> 0.689233281
    {0x3ffffc0000000000ull, 0x3fe62a42bef4e41aull},  // 1.99902344 -> 0.69265878
    {0x3fffffffffffffffull, 0x3fe62e42fefa39eeull},  // 2 -> 0.693147181
    {0x4000000000000000ull, 0x3fe62e42fefa39efull},  // 2 -> 0.693147181
    {0x4005bf0a8b145769ull, 0x3ff0000000000000ull},  // 2.71828183 -> 1
    {0x4008000000000000ull, 0x3ff193ea7aad030bull},  // 3 -> 1.09861229
    {0x4024000000000000ull, 0x40026bb1bbb55516ull},  // 10 -> 2.30258509
    {0x408f400000000000ull, 0x401ba18a998fffa0ull},  // 1000 -> 6.90775528
    {0x40fe240c9fbe76c9ull, 0x40277281cad8a844ull},  // 123456.789 -> 11.7236465
    {0x54b249ad2594c37dull, 0x406cc845b54b54f2ull},  // 1e+100 -> 230.258509
    {0x7e37e43c8800759cull, 0x4085963447f87fb5ull},  // 1e+300 -> 690.775528
    {0x7fefffffffffffffull, 0x40862e42fefa39efull},  // 1.79769313e+308 -> 709.782713
};

// The same inputs and the bits rc_log2_f64 gives for them.
static const uint64_t log2_golden[][2] = {
    {0x0000000000000001ull, 0xc090c80000000000ull},  // 4.94065646e-324 -> -1074
    {0x0008000000000000ull, 0xc08ff80000000000ull},  // 1.11253693e-308 -> -1023
    {0x0010000000000000ull, 0xc08ff00000000000ull},  // 2.22507386e-308 -> -1022
    {0x01a56e1fc2f8f359ull, 0xc08f24a09f1a8b89ull},  // 1e-300 -> -996.578428
    {0x3ddb7cdfd9d7bdbbull, 0xc0409c1165ec0627ull},  // 1e-10 -> -33.2192809
    {0x3fb999999999999aull, 0xc00a934f0979a371ull},  // 0.1 -> -3.32192809
    {0x3fdfffffffffffffull, 0xbff0000000000001ull},  // 0.5 -> -1
    {0x3fe0000000000000ull, 0xbff0000000000000ull},  // 0.5 -> -1
    {0x3fe6a09e667f3bcdull, 0xbfdffffffffffffeull},  // 0.707106781 -> -0.5
    {0x3feccccccccccccdull, 0xbfc374d65d9e608dull},  // 0.9 -> -0.152003093
    {0x3fefae147ae147aeull, 0xbf8db1f34d2c386dull},  // 0.99 -> -0.0144995697
    {0x3fefffffffffffffull, 0xbca71547652b82feull},  // 1 -> -1.60171325e-16
    {0x3feffffe00000000ull, 0xbeb715481dd5c5d9ull},  // 0.999999046 -> -1.37586186e-06
    {0x3ff0000000000001ull, 0x3cb71547652b82fdull},  // 1 -> 3.2034265e-16
    {0x3ff0000100000000ull, 0x3eb71546ac814f86ull},  // 1.00000095 -> 1.37586055e-06
    {0x3ff0100000000000ull, 0x3f7709c46d7aac77ull},  // 1.00390625 -> 0.00562454919
    {0x3ff00ff972474539ull, 0x3f77005965c750e5ull},  // 1.0039 -> 0.00561556741
    {0x3ff0200000000000ull, 0x3f86fe50b6ef0851ull},  // 1.0078125 -> 0.0112272554
    {0x3ff01f8a0902de01ull, 0x3f86a9e0690b78a3ull},  // 1.0077 -> 0.0110662014
    {0x3ff8000000000000ull, 0x3fe2b803473f7ad1ull},  // 1.5 -> 0.584962501
    {0x3fffe00000000000ull, 0x3fefd1be4c7f2af9ull},  // 1.9921875 -> 0.994353437
    {0x3ffffc0000000000ull, 0x3feffa3a51c9e50full},  // 1.99902344 -> 0.999295387
    {0x3fffffffffffffffull, 0x3fefffffffffffffull},  // 2 -> 1
    {0x4000000000000000ull, 0x3ff0000000000000ull},  // 2 -> 1
    {0x4005bf0a8b145769ull, 0x3ff71547652b82feull},  // 2.71828183 -> 1.44269504
    {0x4008000000000000ull, 0x3ff95c01a39fbd68ull},  // 3 -> 1.5849625
    {0x4024000000000000ull, 0x400a934f0979a371ull},  // 10 -> 3.32192809
    {0x408f400000000000ull, 0x4023ee7b471b3a95ull},  // 1000 -> 9.96578428
    {0x40fe240c9fbe76c9ull, 0x4030e9e4bf2a1cb2ull},  // 123456.789 -> 16.9136466
    {0x54b249ad2594c37dull, 0x4074c315bf6707b1ull},  // 1e+100 -> 332.192809
    {0x7e37e43c8800759cull, 0x408f24a09f1a8b89ull},  // 1e+300 -> 996.578428
    {0x7fefffffffffffffull, 0x4090000000000000ull},  // 1.79769313e+308 -> 1024
};

// x, y and the bits rc_pow_f64 gives for them: ordinary, near 1 with a large power, near overflow,
// subnormal results, a negative x (its result also overflowing or subnormal), a subnormal x,
// negative too, and a subnormal y.
static const uint64_t pow_golden[][3] = {
    {0x4000000000000000ull, 0x3fe0000000000000ull, 0x3ff6a09e667f3bcdull},  // 2 ^ 0.5
    {0x4000000000000000ull, 0xbfe0000000000000ull, 0x3fe6a09e667f3bcdull},  // 2 ^ -0.5
    {0x4024000000000000ull, 0x3fd3333333333333ull, 0x3fffec982d5bb8afull},  // 10 ^ 0.3
    {0x3fe0000000000000ull, 0x400d99999999999aull, 0x3fb3b2c47bff8328ull},  // 0.5 ^ 3.7
    {0x3ff8000000000000ull, 0x4059100000000000ull, 0x4398f9f20247e3a3ull},  // 1.5 ^ 100.25
    {0x4008000000000000ull, 0x3fd54fdf3b645a1dull, 0x3ff7114a92c643f3ull},  // 3 ^ 0.333
    {0x4059000000000000ull, 0xbff8000000000000ull, 0x3f50624dd2f1a9fcull},  // 100 ^ -1.5
    {0x3ff0000000001000ull, 0x4270000000000000ull, 0x4005bf0a8b144c8aull},  // 1 ^ 1.09951e+12
    {0x3fefffffff800000ull, 0xc1e0000000000000ull, 0x401d8e64b94b1741ull},  // 1 ^ -2.14748e+09
    {0x3fefffffffffffffull, 0x43abc16d674ec800ull, 0x35ec6ab6c6b3f2f9ull},  // 1 ^ 1e+18
    {0x3ff000001ad7f29bull, 0xc1fa13b860000000ull, 0x00d14f524d3e7179ull},  // 1 ^ -7e+09
    {0x401c000000000000ull, 0x4072c00000000000ull, 0x7492763cf89c12d4ull},  // 7 ^ 300
    {0x4000000000000000ull, 0x408fffeb851eb852ull, 0x7fefc769e9b9c396ull},  // 2 ^ 1023.99
    {0x4024000000000000ull, 0xc073380000000000ull, 0x0016bd3d7ae36ea2ull},  // 10 ^ -307.5
    {0x4024000000000000ull, 0xc07364cccccccccdull, 0x00000939de0342bdull},  // 10 ^ -310.3
    {0x4024000000000000ull, 0xc074380000000000ull, 0x0000000000000001ull},  // 10 ^ -323.5
    {0x3fe0000000000000ull, 0x4090ca0000000000ull, 0x0000000000000001ull},  // 0.5 ^ 1074.5
    {0xc008000000000000ull, 0x4014000000000000ull, 0xc06e600000000000ull},  // -3 ^ 5
    {0xbff199999999999aull, 0x4059400000000000ull, 0xc0cd9b5637aa9b25ull},  // -1.1 ^ 101
    {0xbfeccccccccccccdull, 0xc069200000000000ull, 0xc1d777cd8efcb7e3ull},  // -0.9 ^ -201
    {0xc000000000000000ull, 0x4090040000000000ull, 0xfff0000000000000ull},  // -2 ^ 1025
    {0xbff8000000000000ull, 0x409f400000000000ull, 0x7ff0000000000000ull},  // -1.5 ^ 2000
    {0xbfe0000000000000ull, 0x4090c40000000000ull, 0x8000000000000002ull},  // -0.5 ^ 1073
    {0xbfe0000000000000ull, 0x4090c80000000000ull, 0x0000000000000001ull},  // -0.5 ^ 1074
    {0x0000000000000123ull, 0x3fd0000000000000ull, 0x2f475d32c4410032ull},  // 1.43773e-321 ^ 0.25
    {0x000012688b70e62bull, 0xbfe0000000000000ull, 0x601dd55745cbb7faull},  // 1e-310 ^ -0.5
    {0x7e37e43c8800759cull, 0x3ff051eb851eb852ull, 0x7f76c8e5ca239078ull},  // 1e+300 ^ 1.02
    {0x01a56e1fc2f8f359ull, 0x3ff051eb851eb852ull, 0x0066789e3750f743ull},  // 1e-300 ^ 1.02
    {0x4008000000000000ull, 0x000012688b70e62bull, 0x3ff0000000000000ull},  // 3 ^ 1e-310
    {0x3fc8000000000000ull, 0xc075e80000000000ull, 0x74d62bf083488780ull},  // 0.1875 ^ -350.5
    {0x40c81cd6c8b43958ull, 0x3f1a36e2eb1c432dull, 0x3ff003dc55da797aull},  // 12345.7 ^ 0.0001
    {0x3f50624dd2f1a9fcull, 0x4049400000000000ull, 0x207a7fdfb3dc0efdull},  // 0.001 ^ 50.5
    {0x3ff4000000000000ull, 0xc0a7700000000000ull, 0x03929499e4033af7ull},  // 1.25 ^ -3000
    {0x4058f9999999999aull, 0x4058f9999999999aull, 0x6967de56080e820eull},  // 99.9 ^ 99.9
    {0x80005c0ab9347ed7ull, 0x3ff0000000000000ull, 0x80005c0ab9347ed7ull},  // -5e-310 ^ 1
    {0x8000000000000018ull, 0x4008000000000000ull, 0x8000000000000000ull},  // -1.18576e-322 ^ 3
    {0x000012688b70e62bull, 0xbff0000000000000ull, 0x7ff0000000000000ull},  // 1e-310 ^ -1
};


// The bits of x, to check a result exactly, signed zeros and all.
static uint64_t bits(double x)
{
    return rc_bitcast_f64(x);
}


// +infinity, which C17 has no literal for without <math.h>.
static double infinity(void)
{
    return rc_bitcast_u64(0x7ff0000000000000ull);
}


// A quiet NaN.
static double not_a_number(void)
{
    return rc_bitcast_u64(0x7ff8000000000000ull);
}


// 2^k, for any k a double has, subnormal powers included.
static double power_of_2(int32_t k)
{
    return k < -1022 ? rc_bitcast_u64(1ull << (k + 1074)) : rc_bitcast_u64((uint64_t)(k + 1023) << 52);
}


// True if x is a NaN, which is never equal to itself.
static bool is_nan(double x)
{
    return !rc_is_finite_f64(x) && x != x;
}


// ---- exp ----

RC_TEST(math_exp_f64, special_values)
{
    RC_CHECK(bits(rc_exp_f64(0.0)), ==, bits(1.0));
    RC_CHECK(bits(rc_exp_f64(-0.0)), ==, bits(1.0));
    RC_CHECK(bits(rc_exp_f64(infinity())), ==, bits(infinity()));
    RC_CHECK(bits(rc_exp_f64(-infinity())), ==, 0ull);
    RC_CHECK_TRUE(is_nan(rc_exp_f64(not_a_number())));
    RC_CHECK_TRUE(is_nan(rc_exp_f64(-not_a_number())));
}


RC_TEST(math_exp_f64, overflow_and_underflow)
{
    double largest = rc_bitcast_u64(0x7fefffffffffffffull);

    // e^709.782712893384 is just under the largest double, and a whisker more is past it
    RC_CHECK_TRUE(rc_is_finite_f64(rc_exp_f64(709.782712893384)));
    RC_CHECK(bits(rc_exp_f64(709.7827128933841)), ==, bits(infinity()));
    RC_CHECK(bits(rc_exp_f64(709.79)), ==, bits(infinity()));
    RC_CHECK(bits(rc_exp_f64(710.0)), ==, bits(infinity()));
    RC_CHECK(bits(rc_exp_f64(largest)), ==, bits(infinity()));

    // Half the smallest subnormal is e^-745.1332, so a little above that rounds up to the smallest
    // subnormal, and a little below to zero
    RC_CHECK(bits(rc_exp_f64(-745.13)), ==, 1ull);
    RC_CHECK(bits(rc_exp_f64(-745.14)), ==, 0ull);
    RC_CHECK(bits(rc_exp_f64(-745.2)), ==, 0ull);
    RC_CHECK(bits(rc_exp_f64(-1000.0)), ==, 0ull);
    RC_CHECK(bits(rc_exp_f64(-largest)), ==, 0ull);
}


RC_TEST(math_exp_f64, readable)
{
    RC_CHECK(rc_exp_f64(1.0), ==, 2.718281828459045);
    RC_CHECK(rc_exp_f64(-1.0), ~=, 0.36787944117144233);
    RC_CHECK(rc_exp_f64(10.0), ~=, 22026.465794806718);

    // ln 2 rounded to a double is a hair under the real thing, not enough to move the result off 2
    RC_CHECK(rc_exp_f64(0.6931471805599453), ==, 2.0);
    RC_CHECK(rc_exp_f64(1e-300), ==, 1.0);
    RC_CHECK(rc_exp_f64(-1e-300), ==, 1.0);
}


RC_TEST(math_exp_f64, golden)
{
    for (uint32_t i = 0; i < RC_COUNT(exp_golden); i++) {
        RC_CHECK(bits(rc_exp_f64(rc_bitcast_u64(exp_golden[i][0]))), ==, exp_golden[i][1]);
    }
}


// ---- log ----

RC_TEST(math_log_f64, special_values)
{
    RC_CHECK(bits(rc_log_f64(0.0)), ==, bits(-infinity()));
    RC_CHECK(bits(rc_log_f64(-0.0)), ==, bits(-infinity()));
    RC_CHECK(bits(rc_log_f64(1.0)), ==, 0ull);
    RC_CHECK(bits(rc_log_f64(infinity())), ==, bits(infinity()));
    RC_CHECK_TRUE(is_nan(rc_log_f64(-1.0)));
    RC_CHECK_TRUE(is_nan(rc_log_f64(-1e-300)));
    RC_CHECK_TRUE(is_nan(rc_log_f64(rc_bitcast_u64(0x8000000000000001ull))));
    RC_CHECK_TRUE(is_nan(rc_log_f64(-infinity())));
    RC_CHECK_TRUE(is_nan(rc_log_f64(not_a_number())));
}


RC_TEST(math_log_f64, readable)
{
    RC_CHECK(rc_log_f64(2.718281828459045), ==, 1.0);
    RC_CHECK(rc_log_f64(2.0), ==, 0.6931471805599453);
    RC_CHECK(rc_log_f64(0.5), ==, -0.6931471805599453);
    RC_CHECK(rc_log_f64(10.0), ~=, 2.302585092994046);
    RC_CHECK(rc_log_f64(1e300), ~=, 690.7755278982137);

    // Either side of 1 the result is about x - 1, with the right sign. log(1 + 2^-52) is
    // 2^-52 - 2^-105, and the doubles just under 2^-52 are fine enough to tell
    RC_CHECK(rc_log_f64(1.0 + 0x1p-52), ==, 0x1.fffffffffffffp-53);
    RC_CHECK(rc_log_f64(1.0 - 0x1p-53), ==, -0x1p-53);
    RC_CHECK(rc_log_f64(1.0 + 0x1p-30), ~=, 0x1p-30);
}


RC_TEST(math_log_f64, golden)
{
    for (uint32_t i = 0; i < RC_COUNT(log_golden); i++) {
        RC_CHECK(bits(rc_log_f64(rc_bitcast_u64(log_golden[i][0]))), ==, log_golden[i][1]);
    }
}


// ---- log2 ----

RC_TEST(math_log2_f64, special_values)
{
    RC_CHECK(bits(rc_log2_f64(0.0)), ==, bits(-infinity()));
    RC_CHECK(bits(rc_log2_f64(-0.0)), ==, bits(-infinity()));
    RC_CHECK(bits(rc_log2_f64(1.0)), ==, 0ull);
    RC_CHECK(bits(rc_log2_f64(infinity())), ==, bits(infinity()));
    RC_CHECK_TRUE(is_nan(rc_log2_f64(-1.0)));
    RC_CHECK_TRUE(is_nan(rc_log2_f64(-1e-300)));
    RC_CHECK_TRUE(is_nan(rc_log2_f64(rc_bitcast_u64(0x8000000000000001ull))));
    RC_CHECK_TRUE(is_nan(rc_log2_f64(-infinity())));
    RC_CHECK_TRUE(is_nan(rc_log2_f64(not_a_number())));
}


RC_TEST(math_log2_f64, powers_of_2)
{
    for (int32_t k = -1074; k <= 1023; k++) {
        RC_CHECK(rc_log2_f64(power_of_2(k)), ==, (double)k);
    }
}


RC_TEST(math_log2_f64, readable)
{
    RC_CHECK(rc_log2_f64(8.0), ==, 3.0);
    RC_CHECK(rc_log2_f64(0.125), ==, -3.0);
    RC_CHECK(rc_log2_f64(10.0), ~=, 3.321928094887362);
    RC_CHECK(rc_log2_f64(3.0), ~=, 1.584962500721156);
    RC_CHECK(rc_log2_f64(1.0 + 0x1p-30), ~=, 0x1.71547652p-30);
}


RC_TEST(math_log2_f64, golden)
{
    for (uint32_t i = 0; i < RC_COUNT(log2_golden); i++) {
        RC_CHECK(bits(rc_log2_f64(rc_bitcast_u64(log2_golden[i][0]))), ==, log2_golden[i][1]);
    }
}


// ---- pow ----

RC_TEST(math_pow_f64, zero_power)
{
    double bases[] = {0.0, -0.0, 1.0, -1.0, 2.5, -3.0, 1e-310, infinity(), -infinity(), not_a_number()};

    // Anything to the power +-0 is 1, even NaN
    for (uint32_t i = 0; i < RC_COUNT(bases); i++) {
        RC_CHECK(bits(rc_pow_f64(bases[i], 0.0)), ==, bits(1.0));
        RC_CHECK(bits(rc_pow_f64(bases[i], -0.0)), ==, bits(1.0));
    }
}


RC_TEST(math_pow_f64, base_one)
{
    double powers[] = {0.0, 0.5, -7.0, 1e300, -1e300, 1e-310, infinity(), -infinity(), not_a_number()};

    // 1 to any power is 1, even NaN; -1 to an infinite power is too
    for (uint32_t i = 0; i < RC_COUNT(powers); i++) {
        RC_CHECK(bits(rc_pow_f64(1.0, powers[i])), ==, bits(1.0));
    }

    RC_CHECK(bits(rc_pow_f64(-1.0, infinity())), ==, bits(1.0));
    RC_CHECK(bits(rc_pow_f64(-1.0, -infinity())), ==, bits(1.0));
}


RC_TEST(math_pow_f64, zero_base)
{
    // A negative odd power keeps the zero's sign on the infinity
    RC_CHECK(bits(rc_pow_f64(0.0, -3.0)), ==, bits(infinity()));
    RC_CHECK(bits(rc_pow_f64(-0.0, -3.0)), ==, bits(-infinity()));
    RC_CHECK(bits(rc_pow_f64(-0.0, -1.0)), ==, bits(-infinity()));

    // Any other negative power gives +infinity
    RC_CHECK(bits(rc_pow_f64(-0.0, -2.0)), ==, bits(infinity()));
    RC_CHECK(bits(rc_pow_f64(-0.0, -0.5)), ==, bits(infinity()));
    RC_CHECK(bits(rc_pow_f64(0.0, -4.5)), ==, bits(infinity()));
    RC_CHECK(bits(rc_pow_f64(-0.0, -1e300)), ==, bits(infinity()));
    RC_CHECK(bits(rc_pow_f64(0.0, -infinity())), ==, bits(infinity()));
    RC_CHECK(bits(rc_pow_f64(-0.0, -infinity())), ==, bits(infinity()));

    // A positive odd power keeps the zero's sign, and any other gives +0
    RC_CHECK(bits(rc_pow_f64(0.0, 3.0)), ==, 0ull);
    RC_CHECK(bits(rc_pow_f64(-0.0, 3.0)), ==, 0x8000000000000000ull);
    RC_CHECK(bits(rc_pow_f64(-0.0, 1.0)), ==, 0x8000000000000000ull);
    RC_CHECK(bits(rc_pow_f64(-0.0, 2.0)), ==, 0ull);
    RC_CHECK(bits(rc_pow_f64(-0.0, 0.5)), ==, 0ull);
    RC_CHECK(bits(rc_pow_f64(-0.0, 1e300)), ==, 0ull);
    RC_CHECK(bits(rc_pow_f64(-0.0, infinity())), ==, 0ull);
}


RC_TEST(math_pow_f64, infinite_power)
{
    double small[] = {0.5, -0.5, 1e-300, -0.9999999999999999, 1e-310};
    double large[] = {2.0, -2.0, 1.0000000000000002, 1e300, infinity(), -infinity()};

    for (uint32_t i = 0; i < RC_COUNT(small); i++) {
        RC_CHECK(bits(rc_pow_f64(small[i], -infinity())), ==, bits(infinity()));
        RC_CHECK(bits(rc_pow_f64(small[i], infinity())), ==, 0ull);
    }

    for (uint32_t i = 0; i < RC_COUNT(large); i++) {
        RC_CHECK(bits(rc_pow_f64(large[i], -infinity())), ==, 0ull);
        RC_CHECK(bits(rc_pow_f64(large[i], infinity())), ==, bits(infinity()));
    }
}


RC_TEST(math_pow_f64, infinite_base)
{
    // -infinity to an odd power keeps its sign, on the infinity or the zero
    RC_CHECK(bits(rc_pow_f64(-infinity(), -3.0)), ==, 0x8000000000000000ull);
    RC_CHECK(bits(rc_pow_f64(-infinity(), -1.0)), ==, 0x8000000000000000ull);
    RC_CHECK(bits(rc_pow_f64(-infinity(), -2.0)), ==, 0ull);
    RC_CHECK(bits(rc_pow_f64(-infinity(), -0.5)), ==, 0ull);
    RC_CHECK(bits(rc_pow_f64(-infinity(), -1e300)), ==, 0ull);
    RC_CHECK(bits(rc_pow_f64(-infinity(), 3.0)), ==, bits(-infinity()));
    RC_CHECK(bits(rc_pow_f64(-infinity(), 1.0)), ==, bits(-infinity()));
    RC_CHECK(bits(rc_pow_f64(-infinity(), 2.0)), ==, bits(infinity()));
    RC_CHECK(bits(rc_pow_f64(-infinity(), 0.5)), ==, bits(infinity()));
    RC_CHECK(bits(rc_pow_f64(-infinity(), 1e300)), ==, bits(infinity()));

    // +infinity has no sign to keep
    RC_CHECK(bits(rc_pow_f64(infinity(), -3.0)), ==, 0ull);
    RC_CHECK(bits(rc_pow_f64(infinity(), -1e-310)), ==, 0ull);
    RC_CHECK(bits(rc_pow_f64(infinity(), 3.0)), ==, bits(infinity()));
    RC_CHECK(bits(rc_pow_f64(infinity(), 1e-310)), ==, bits(infinity()));
}


RC_TEST(math_pow_f64, negative_base)
{
    double above_2_52 = 4503599627370497.0;
    double below_2_53 = 9007199254740991.0;
    double two_53 = 9007199254740992.0;
    double just_under_1 = 1.0 - 0x1p-53;

    // No real power for a power that isn't whole, however close: 2^52 - 1/2 is the last double
    // with a fraction
    RC_CHECK_TRUE(is_nan(rc_pow_f64(-2.0, 0.5)));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(-1.0, -0.5)));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(-1e-300, 1.5)));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(-1e-310, 1.5)));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(-1.0, 4503599627370495.5)));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(-1.5, 4503599627370494.5)));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(-2.0, 1e-310)));

    // Whole powers give |x|^y, negated for an odd one
    RC_CHECK(rc_pow_f64(-2.0, 3.0), ==, -8.0);
    RC_CHECK(rc_pow_f64(-2.0, 2.0), ==, 4.0);
    RC_CHECK(rc_pow_f64(-2.0, -3.0), ==, -0.125);
    RC_CHECK(rc_pow_f64(-2.0, -2.0), ==, 0.25);
    RC_CHECK(rc_pow_f64(-1.0, above_2_52), ==, -1.0);
    RC_CHECK(rc_pow_f64(-1.0, above_2_52 - 1.0), ==, 1.0);
    RC_CHECK(rc_pow_f64(-1.0, below_2_53), ==, -1.0);
    RC_CHECK(rc_pow_f64(-1.0, -below_2_53), ==, -1.0);

    // From 2^53 every double is even
    RC_CHECK(rc_pow_f64(-1.0, two_53), ==, 1.0);
    RC_CHECK(rc_pow_f64(-1.0, two_53 + 2.0), ==, 1.0);
    RC_CHECK(rc_pow_f64(-1.0, 1e300), ==, 1.0);
    RC_CHECK(rc_pow_f64(-2.0, 1e300), ==, infinity());
    RC_CHECK(bits(rc_pow_f64(-0.5, 1e300)), ==, 0ull);
    RC_CHECK(bits(rc_pow_f64(-2.0, -1e300)), ==, 0ull);

    // Just under 1 to the power of about 2^52 or 2^53 is about e^-1/2 or e^-1
    RC_CHECK(rc_pow_f64(-just_under_1, above_2_52), ~=, -0.6065306597126334);
    RC_CHECK(rc_pow_f64(-just_under_1, below_2_53), ~=, -0.36787944117144233);
    RC_CHECK(rc_pow_f64(-just_under_1, two_53), ~=, 0.36787944117144233);
}


RC_TEST(math_pow_f64, sign_symmetry)
{
    double bases[] = {1e-5, 0.3, 1.7, 3.0, 10.5, 123.25, 1e-310, 1e200};
    double powers[] = {-101.0, -7.0, -2.0, -1.0, 1.0, 2.0, 3.0, 15.0, 100.0, 101.0};

    // (-x)^y is exactly x^y for an even y and -(x^y) for an odd one
    for (uint32_t i = 0; i < RC_COUNT(bases); i++) {
        for (uint32_t j = 0; j < RC_COUNT(powers); j++) {
            double positive = rc_pow_f64(bases[i], powers[j]);
            bool odd = rc_trunc_f64(powers[j] * 0.5) != powers[j] * 0.5;

            RC_CHECK(bits(rc_pow_f64(-bases[i], powers[j])), ==, bits(odd ? -positive : positive));
        }
    }
}


RC_TEST(math_pow_f64, nan)
{
    RC_CHECK_TRUE(is_nan(rc_pow_f64(not_a_number(), 1.0)));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(not_a_number(), infinity())));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(2.0, not_a_number())));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(-1.0, not_a_number())));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(0.0, not_a_number())));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(infinity(), not_a_number())));
    RC_CHECK_TRUE(is_nan(rc_pow_f64(not_a_number(), not_a_number())));
}


RC_TEST(math_pow_f64, overflow_and_underflow)
{
    // Overflow gives an infinity, negative for a negative base and an odd power
    RC_CHECK(bits(rc_pow_f64(2.0, 1024.0)), ==, bits(infinity()));
    RC_CHECK(bits(rc_pow_f64(-2.0, 1025.0)), ==, bits(-infinity()));
    RC_CHECK(bits(rc_pow_f64(-2.0, 1024.0)), ==, bits(infinity()));
    RC_CHECK(bits(rc_pow_f64(10.0, 309.0)), ==, bits(infinity()));
    RC_CHECK(bits(rc_pow_f64(0.5, -1024.0)), ==, bits(infinity()));
    RC_CHECK(bits(rc_pow_f64(-0.5, -1025.0)), ==, bits(-infinity()));
    RC_CHECK(bits(rc_pow_f64(1.0000001, 1e10)), ==, bits(infinity()));
    RC_CHECK(rc_pow_f64(2.0, 1023.0), ==, 0x1p1023);
    RC_CHECK(rc_pow_f64(-2.0, 1023.0), ==, -0x1p1023);

    // Underflow gives a zero, signed the same way
    RC_CHECK(bits(rc_pow_f64(2.0, -1080.0)), ==, 0ull);
    RC_CHECK(bits(rc_pow_f64(-2.0, -1081.0)), ==, 0x8000000000000000ull);
    RC_CHECK(bits(rc_pow_f64(-2.0, -1080.0)), ==, 0ull);
    RC_CHECK(bits(rc_pow_f64(10.0, -400.0)), ==, 0ull);
    RC_CHECK(bits(rc_pow_f64(1e300, -2.0)), ==, 0ull);
    RC_CHECK(bits(rc_pow_f64(-1e300, -3.0)), ==, 0x8000000000000000ull);
    RC_CHECK(bits(rc_pow_f64(-1e-300, 3.0)), ==, 0x8000000000000000ull);
    RC_CHECK(bits(rc_pow_f64(0.9999999, 1e10)), ==, 0ull);
}


RC_TEST(math_pow_f64, subnormal_results)
{
    RC_CHECK(rc_pow_f64(2.0, -1074.0), ==, power_of_2(-1074));
    RC_CHECK(rc_pow_f64(0.5, 1030.0), ==, power_of_2(-1030));
    RC_CHECK(rc_pow_f64(-0.5, 1031.0), ==, -power_of_2(-1031));
    RC_CHECK(rc_pow_f64(-2.0, -1073.0), ==, -power_of_2(-1073));

    // 2^-1075 is halfway to the smallest subnormal and rounds to the even zero, but 2^-1073.5
    // rounds to 2^-1074
    RC_CHECK(bits(rc_pow_f64(2.0, -1075.0)), ==, 0ull);
    RC_CHECK(rc_pow_f64(2.0, -1073.5), ==, power_of_2(-1074));
    RC_CHECK(rc_pow_f64(10.0, -310.0), >, 0.0);
    RC_CHECK(rc_pow_f64(10.0, -310.0), <, power_of_2(-1022));
}


RC_TEST(math_pow_f64, subnormal_base)
{
    double smallest = power_of_2(-1074);

    RC_CHECK(rc_pow_f64(smallest, 1.0), ==, smallest);
    RC_CHECK(rc_pow_f64(smallest, 0.5), ==, power_of_2(-537));
    RC_CHECK(rc_pow_f64(smallest, -0.5), ==, power_of_2(537));
    RC_CHECK(rc_pow_f64(-smallest, 1.0), ==, -smallest);
    RC_CHECK(bits(rc_pow_f64(smallest, -1.0)), ==, bits(infinity()));
    RC_CHECK(bits(rc_pow_f64(-smallest, -1.0)), ==, bits(-infinity()));
    RC_CHECK(bits(rc_pow_f64(-smallest, 2.0)), ==, 0ull);
    RC_CHECK(rc_pow_f64(power_of_2(-1040), 0.25), ==, power_of_2(-260));
}


RC_TEST(math_pow_f64, exact_results)
{
    double bases[] = {1e-300, 0.1, 0.75, 3.0, 1e300, power_of_2(-1060)};

    // A result a double can hold exactly comes out exactly
    RC_CHECK(rc_pow_f64(2.0, 10.0), ==, 1024.0);
    RC_CHECK(rc_pow_f64(10.0, 22.0), ==, 1e22);
    RC_CHECK(rc_pow_f64(3.0, 33.0), ==, 5559060566555523.0);
    RC_CHECK(rc_pow_f64(5.0, 3.0), ==, 125.0);
    RC_CHECK(rc_pow_f64(1.5, 2.0), ==, 2.25);
    RC_CHECK(rc_pow_f64(9.0, 0.5), ==, 3.0);
    RC_CHECK(rc_pow_f64(16.0, 0.25), ==, 2.0);
    RC_CHECK(rc_pow_f64(0.25, -0.5), ==, 2.0);
    RC_CHECK(rc_pow_f64(2.0, -1.0), ==, 0.5);
    for (uint32_t i = 0; i < RC_COUNT(bases); i++) {
        RC_CHECK(rc_pow_f64(bases[i], 1.0), ==, bases[i]);
    }
}


RC_TEST(math_pow_f64, golden)
{
    for (uint32_t i = 0; i < RC_COUNT(pow_golden); i++) {
        double x = rc_bitcast_u64(pow_golden[i][0]);
        double y = rc_bitcast_u64(pow_golden[i][1]);

        RC_CHECK(bits(rc_pow_f64(x, y)), ==, pow_golden[i][2]);
    }
}
