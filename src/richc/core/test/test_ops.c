// test_ops.c
//
// Tests for the bit operations in ops.h.

#include "richc/ops.h"

#include "richc/test.h"


RC_TEST(ops, clz)
{
    RC_CHECK(rc_clz_u32(0), ==, 32u);
    RC_CHECK(rc_clz_u32(1), ==, 31u);
    RC_CHECK(rc_clz_u32(0x80000000u), ==, 0u);
    RC_CHECK(rc_clz_u32(0x00010000u), ==, 15u);
    RC_CHECK(rc_clz_u32(UINT32_MAX), ==, 0u);
    RC_CHECK(rc_clz_u64(0), ==, 64u);
    RC_CHECK(rc_clz_u64(1), ==, 63u);
    RC_CHECK(rc_clz_u64(0x8000000000000000ull), ==, 0u);
    RC_CHECK(rc_clz_u64(0x0000000100000000ull), ==, 31u);
}


RC_TEST(ops, ctz)
{
    RC_CHECK(rc_ctz_u32(0), ==, 32u);
    RC_CHECK(rc_ctz_u32(1), ==, 0u);
    RC_CHECK(rc_ctz_u32(0x80000000u), ==, 31u);
    RC_CHECK(rc_ctz_u32(0x00010000u), ==, 16u);
    RC_CHECK(rc_ctz_u32(UINT32_MAX), ==, 0u);
    RC_CHECK(rc_ctz_u64(0), ==, 64u);
    RC_CHECK(rc_ctz_u64(1), ==, 0u);
    RC_CHECK(rc_ctz_u64(0x8000000000000000ull), ==, 63u);
    RC_CHECK(rc_ctz_u64(0x0000000100000000ull), ==, 32u);
}


RC_TEST(ops, every_single_bit)
{
    for (uint32_t i = 0; i < 32; i++) {
        RC_CHECK(rc_clz_u32(1u << i), ==, 31 - i);
        RC_CHECK(rc_ctz_u32(1u << i), ==, i);
        RC_CHECK(rc_popcount_u32(1u << i), ==, 1u);
    }
    for (uint32_t i = 0; i < 64; i++) {
        RC_CHECK(rc_clz_u64(1ull << i), ==, 63 - i);
        RC_CHECK(rc_ctz_u64(1ull << i), ==, i);
    }
}


RC_TEST(ops, popcount)
{
    RC_CHECK(rc_popcount_u32(0), ==, 0u);
    RC_CHECK(rc_popcount_u32(UINT32_MAX), ==, 32u);
    RC_CHECK(rc_popcount_u32(0x55555555u), ==, 16u);
    RC_CHECK(rc_popcount_u32(0x80000001u), ==, 2u);
}
