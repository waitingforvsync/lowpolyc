// test_spinlock.c
//
// Tests for rc_spinlock, on one thread. Threads contending for one are in test_thread.c.

#include "richc/test.h"
#include "richc/thread/spinlock.h"


RC_TEST(spinlock, zeroed_is_unlocked)
{
    rc_spinlock lock = {0};

    RC_CHECK_TRUE(rc_spinlock_try_lock(&lock));
    rc_spinlock_unlock(&lock);
}


RC_TEST(spinlock, try_lock_fails_while_held)
{
    rc_spinlock lock = {0};

    rc_spinlock_lock(&lock);
    RC_CHECK_FALSE(rc_spinlock_try_lock(&lock));
    RC_CHECK_FALSE(rc_spinlock_try_lock(&lock));
    rc_spinlock_unlock(&lock);
    RC_CHECK_TRUE(rc_spinlock_try_lock(&lock));
    RC_CHECK_FALSE(rc_spinlock_try_lock(&lock));
    rc_spinlock_unlock(&lock);
}


RC_TEST(spinlock, relocks_after_unlock)
{
    rc_spinlock lock = {0};

    for (uint32_t i = 0; i < 3; i++) {
        rc_spinlock_lock(&lock);
        rc_spinlock_unlock(&lock);
    }

    RC_CHECK_TRUE(rc_spinlock_try_lock(&lock));
    rc_spinlock_unlock(&lock);
}


RC_TEST(spinlock, pause_returns)
{
    rc_spin_pause();
}
