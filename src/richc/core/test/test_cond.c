// test_cond.c
//
// Tests for rc_cond, on one thread, where nothing can signal a waiter, so only timeouts end a
// wait. Threads signalling each other come with the threads.

#include "richc/test.h"
#include "richc/thread/cond.h"
#include "richc/time.h"


RC_TEST(cond, signals_with_no_waiters_do_nothing)
{
    rc_cond cond = {0};

    rc_cond_signal(&cond);
    rc_cond_broadcast(&cond);
    rc_cond_signal(&cond);
}


RC_TEST(cond, wait_for_runs_out_with_the_mutex_held)
{
    rc_cond cond = {0};
    rc_mutex mutex = {0};
    uint64_t start = 0;
    bool woken = false;

    rc_mutex_lock(&mutex);
    start = rc_time_now_ns();

    // A wait can end early for no reason, so loop as a real waiter would
    do {
        woken = rc_cond_wait_for(&cond, &mutex, 2000000u);
    } while (woken && rc_time_now_ns() - start < 2000000u);

    RC_CHECK_FALSE(woken);
    RC_CHECK(rc_time_now_ns() - start, >=, 2000000u);
    RC_CHECK_FALSE(rc_mutex_try_lock(&mutex));
    rc_mutex_unlock(&mutex);
}


RC_TEST(cond, a_zero_timeout_runs_out)
{
    rc_cond cond = {0};
    rc_mutex mutex = {0};

    rc_mutex_lock(&mutex);
    RC_CHECK_FALSE(rc_cond_wait_for(&cond, &mutex, 0));
    rc_mutex_unlock(&mutex);
}
