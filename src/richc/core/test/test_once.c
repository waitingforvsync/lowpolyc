// test_once.c
//
// Tests for rc_once, on one thread. Threads racing to run one are in test_thread.c.

#include "richc/test.h"
#include "richc/thread/once.h"


// Count a call in the uint32_t at context.
static void count_call(void *context)
{
    uint32_t *calls = context;

    (*calls)++;
}


RC_TEST(once, runs_only_the_first_time)
{
    rc_once once = {0};
    uint32_t calls = 0;

    rc_once_call(&once, count_call, &calls);
    RC_CHECK(calls, ==, 1);
    rc_once_call(&once, count_call, &calls);
    rc_once_call(&once, count_call, &calls);
    RC_CHECK(calls, ==, 1);
}


RC_TEST(once, each_once_runs_for_itself)
{
    rc_once first = {0};
    rc_once second = {0};
    uint32_t first_calls = 0;
    uint32_t second_calls = 0;

    rc_once_call(&first, count_call, &first_calls);
    rc_once_call(&second, count_call, &second_calls);
    rc_once_call(&first, count_call, &first_calls);
    RC_CHECK(first_calls, ==, 1);
    RC_CHECK(second_calls, ==, 1);
}
