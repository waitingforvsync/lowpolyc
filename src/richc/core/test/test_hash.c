// test_hash.c
//
// Tests for the hashes.

#include "richc/hash.h"
#include "richc/ops.h"

#include "richc/test.h"


RC_TEST(hash, integers)
{
    RC_CHECK(rc_hash_u32(0xDEADBEEFu), ==, rc_hash_u32(0xDEADBEEFu));
    RC_CHECK(rc_hash_u64(0x0123456789ABCDEFull), ==, rc_hash_u64(0x0123456789ABCDEFull));
    RC_CHECK(rc_hash_u32(0u), !=, rc_hash_u32(1u));
    RC_CHECK(rc_hash_u64(0u), !=, rc_hash_u64(1u));

    // The signed hashes hash the bit pattern
    RC_CHECK(rc_hash_i32(-1), ==, rc_hash_u32(0xFFFFFFFFu));
    RC_CHECK(rc_hash_i64(-1), ==, rc_hash_u64(0xFFFFFFFFFFFFFFFFull));
}


RC_TEST(hash, floats)
{
    RC_CHECK(rc_hash_f32(3.14f), ==, rc_hash_f32(3.14f));
    RC_CHECK(rc_hash_f64(3.14), ==, rc_hash_f64(3.14));
    RC_CHECK(rc_hash_f32(1.0f), !=, rc_hash_f32(2.0f));

    // -0.0 and +0.0 compare equal, so hash alike
    RC_CHECK(rc_hash_f32(-0.0f), ==, rc_hash_f32(0.0f));
    RC_CHECK(rc_hash_f64(-0.0), ==, rc_hash_f64(0.0));
}


RC_TEST(hash, pointers)
{
    int32_t x = 0;

    RC_CHECK(rc_hash_ptr(&x), ==, rc_hash_ptr(&x));
    RC_CHECK(rc_hash_ptr(NULL), ==, rc_hash_u64(0u));
}


RC_TEST(hash, bytes_and_str)
{
    uint8_t data[] = {1, 2, 3, 4};

    RC_CHECK(rc_hash_bytes(data, 4), ==, rc_hash_bytes(data, 4));
    RC_CHECK(rc_hash_str(RC_STR("hello")), ==, rc_hash_bytes("hello", 5));
    RC_CHECK(rc_hash_str(RC_STR("hello")), !=, rc_hash_str(RC_STR("world")));

    // Nothing hashes to 0, however it is spelt
    RC_CHECK(rc_hash_bytes(NULL, 0), ==, 0u);
    RC_CHECK(rc_hash_str((rc_str) {0}), ==, 0u);
}


RC_TEST(hash, murmur3_vectors)
{
    // Murmur3's values for seed 0, with tails of 0 to 3 bytes
    RC_CHECK(rc_hash_str(RC_STR("abcd")), ==, 0x43ED676Au);
    RC_CHECK(rc_hash_str(RC_STR("hello")), ==, 0x248BFA47u);
    RC_CHECK(rc_hash_str(RC_STR("ab")), ==, 0x9BBFD75Fu);
    RC_CHECK(rc_hash_str(RC_STR("Hello, world!")), ==, 0xC0363E43u);
    RC_CHECK(rc_hash_str(RC_STR("The quick brown fox jumps over the lazy dog")), ==, 0x2E4FF723u);
}


RC_TEST(hash, unaligned)
{
    // The same bytes hash alike wherever they start
    uint8_t buffer[16] = {0};

    for (uint32_t offset = 0; offset < 4; offset++) {
        for (uint32_t i = 0; i < 5; i++) {
            buffer[offset + i] = (uint8_t)("hello"[i]);
        }
        RC_CHECK(rc_hash_bytes(buffer + offset, 5), ==, 0x248BFA47u);
    }
}


RC_TEST(hash, combine)
{
    uint32_t a = rc_hash_u32(1u);
    uint32_t b = rc_hash_u32(2u);

    RC_CHECK(rc_hash_combine(a, b), ==, rc_hash_combine(a, b));
    RC_CHECK(rc_hash_combine(a, b), !=, rc_hash_combine(b, a));

    // Zeros neither stay zero nor repeat, so all-zero keys of different lengths differ
    uint32_t zero2 = rc_hash_combine(0u, 0u);
    uint32_t zero3 = rc_hash_combine(zero2, 0u);
    RC_CHECK(zero2, !=, 0u);
    RC_CHECK(zero3, !=, zero2);
}


RC_TEST(hash, combine_mixes_every_bit)
{
    // Flipping any one bit of either input flips several output bits, about half on average
    uint32_t seed = rc_hash_u32(7u);
    uint32_t hash = rc_hash_u32(8u);
    uint32_t base = rc_hash_combine(seed, hash);

    for (uint32_t bit = 0; bit < 32; bit++) {
        RC_CHECK(rc_popcount_u32(base ^ rc_hash_combine(seed ^ (1u << bit), hash)), >=, 6u);
        RC_CHECK(rc_popcount_u32(base ^ rc_hash_combine(seed, hash ^ (1u << bit))), >=, 6u);
    }
}
