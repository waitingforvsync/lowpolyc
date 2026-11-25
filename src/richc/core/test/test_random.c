// test_random.c
//
// Tests for rc_random.

#include "richc/random.h"

#include "richc/test.h"


RC_TEST(random, known_draws)
{
    // splitmix32's first draws, worked out independently, so the stream is pinned bit for bit
    rc_random zero = rc_random_make(0);
    rc_random seeded = rc_random_make(0x1234abcdu);
    rc_random top = rc_random_make(0xffffffffu);

    RC_CHECK(rc_random_next(&zero), ==, 0x64625032u);
    RC_CHECK(rc_random_next(&zero), ==, 0xd9c0799cu);
    RC_CHECK(rc_random_next(&zero), ==, 0xaf362e10u);
    RC_CHECK(rc_random_next(&zero), ==, 0x7fa88912u);
    RC_CHECK(rc_random_next(&seeded), ==, 0xae06d8a6u);
    RC_CHECK(rc_random_next(&seeded), ==, 0x540bb4f6u);
    RC_CHECK(rc_random_next(&top), ==, 0xeb721c8au);
    RC_CHECK(rc_random_next(&top), ==, 0xffe8bd34u);
}


RC_TEST(random, zero_value_is_seed_zero)
{
    rc_random zero = {0};
    rc_random made = rc_random_make(0);

    for (uint32_t i = 0; i < 16; i++) {
        RC_CHECK(rc_random_next(&zero), ==, rc_random_next(&made));
    }
}


RC_TEST(random, same_seed_same_stream)
{
    rc_random a = rc_random_make(0x1234abcdu);
    rc_random b = rc_random_make(0x1234abcdu);

    for (uint32_t i = 0; i < 16; i++) {
        RC_CHECK(rc_random_next(&a), ==, rc_random_next(&b));
    }
}


RC_TEST(random, bits_are_balanced)
{
    // Over 4096 draws each bit should be set about half the time; 1700 to 2400 is a wide margin
    rc_random random = rc_random_make(42);
    uint32_t set[32] = {0};
    bool balanced = true;

    for (uint32_t i = 0; i < 4096; i++) {
        uint32_t draw = rc_random_next(&random);

        for (uint32_t bit = 0; bit < 32; bit++) {
            set[bit] += (draw >> bit) & 1u;
        }
    }

    for (uint32_t bit = 0; bit < 32; bit++) {
        balanced = balanced && set[bit] > 1700 && set[bit] < 2400;
    }
    RC_CHECK_TRUE(balanced);
}
