// test_mutex.c
//
// Tests for rc_mutex, on one thread. Threads contending for one are in test_thread.c.

#include "richc/test.h"
#include "richc/thread/mutex.h"


RC_TEST(mutex, zeroed_is_unlocked)
{
    rc_mutex mutex = {0};

    RC_CHECK_TRUE(rc_mutex_try_lock(&mutex));
    rc_mutex_unlock(&mutex);
}


RC_TEST(mutex, try_lock_fails_while_held)
{
    rc_mutex mutex = {0};

    rc_mutex_lock(&mutex);
    RC_CHECK_FALSE(rc_mutex_try_lock(&mutex));
    rc_mutex_unlock(&mutex);
    RC_CHECK_TRUE(rc_mutex_try_lock(&mutex));
    RC_CHECK_FALSE(rc_mutex_try_lock(&mutex));
    rc_mutex_unlock(&mutex);
}


RC_TEST(mutex, relocks_after_unlock)
{
    rc_mutex mutex = {0};

    for (uint32_t i = 0; i < 3; i++) {
        rc_mutex_lock(&mutex);
        rc_mutex_unlock(&mutex);
    }

    RC_CHECK_TRUE(rc_mutex_try_lock(&mutex));
    rc_mutex_unlock(&mutex);
}
