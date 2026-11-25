// test_time.c
//
// Tests for the monotonic clock.

#include "richc/time.h"

#include "richc/test.h"


RC_TEST(time, never_goes_backwards)
{
    uint64_t before = rc_time_now_ns();
    bool forwards = true;

    for (uint32_t i = 0; i < 100000; i++) {
        uint64_t now = rc_time_now_ns();

        forwards = forwards && now >= before;
        before = now;
    }
    RC_CHECK_TRUE(forwards);
}


RC_TEST(time, moves_on)
{
    // Spin until the clock ticks, with a cap so a stuck clock fails rather than hangs
    uint64_t start = rc_time_now_ns();
    uint64_t now = start;

    for (uint32_t i = 0; i < 100000000 && now == start; i++) {
        now = rc_time_now_ns();
    }
    RC_CHECK(now, >, start);
}
