// test_semaphore.c
//
// Tests for rc_semaphore, on one thread. Threads posting to each other are in test_thread.c.

#include "richc/test.h"
#include "richc/thread/semaphore.h"
#include "richc/time.h"


RC_TEST(semaphore, zeroed_holds_nothing)
{
    rc_semaphore semaphore = {0};

    RC_CHECK_FALSE(rc_semaphore_try_wait(&semaphore));
}


RC_TEST(semaphore, make_starts_at_its_count)
{
    rc_semaphore semaphore = rc_semaphore_make(2);

    RC_CHECK_TRUE(rc_semaphore_try_wait(&semaphore));
    RC_CHECK_TRUE(rc_semaphore_try_wait(&semaphore));
    RC_CHECK_FALSE(rc_semaphore_try_wait(&semaphore));
}


RC_TEST(semaphore, post_adds_to_the_count)
{
    rc_semaphore semaphore = {0};

    rc_semaphore_post(&semaphore, 3);
    rc_semaphore_wait(&semaphore);
    RC_CHECK_TRUE(rc_semaphore_wait_for(&semaphore, 0));
    RC_CHECK_TRUE(rc_semaphore_try_wait(&semaphore));
    RC_CHECK_FALSE(rc_semaphore_try_wait(&semaphore));

    rc_semaphore_post(&semaphore, 1);
    rc_semaphore_post(&semaphore, 0);
    RC_CHECK_TRUE(rc_semaphore_try_wait(&semaphore));
    RC_CHECK_FALSE(rc_semaphore_try_wait(&semaphore));
}


RC_TEST(semaphore, wait_for_runs_out_on_an_empty_count)
{
    rc_semaphore semaphore = {0};
    uint64_t start = rc_time_now_ns();

    RC_CHECK_FALSE(rc_semaphore_wait_for(&semaphore, 2000000u));
    RC_CHECK(rc_time_now_ns() - start, >=, 2000000u);
    RC_CHECK_FALSE(rc_semaphore_wait_for(&semaphore, 0));
    RC_CHECK_FALSE(rc_semaphore_try_wait(&semaphore));
}
