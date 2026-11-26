// test_rwlock.c
//
// Tests for rc_rwlock, on one thread. Readers and writers on threads are in test_thread.c.

#include "richc/test.h"
#include "richc/thread/rwlock.h"


RC_TEST(rwlock, zeroed_is_unlocked)
{
    rc_rwlock lock = {0};

    RC_CHECK_TRUE(rc_rwlock_try_write_lock(&lock));
    rc_rwlock_write_unlock(&lock);
    RC_CHECK_TRUE(rc_rwlock_try_read_lock(&lock));
    rc_rwlock_read_unlock(&lock);
}


RC_TEST(rwlock, readers_share)
{
    rc_rwlock lock = {0};

    rc_rwlock_read_lock(&lock);
    RC_CHECK_TRUE(rc_rwlock_try_read_lock(&lock));
    rc_rwlock_read_lock(&lock);
    RC_CHECK_FALSE(rc_rwlock_try_write_lock(&lock));

    rc_rwlock_read_unlock(&lock);
    rc_rwlock_read_unlock(&lock);
    RC_CHECK_FALSE(rc_rwlock_try_write_lock(&lock));
    rc_rwlock_read_unlock(&lock);
    RC_CHECK_TRUE(rc_rwlock_try_write_lock(&lock));
    rc_rwlock_write_unlock(&lock);
}


RC_TEST(rwlock, a_writer_is_alone)
{
    rc_rwlock lock = {0};

    rc_rwlock_write_lock(&lock);
    RC_CHECK_FALSE(rc_rwlock_try_read_lock(&lock));
    RC_CHECK_FALSE(rc_rwlock_try_write_lock(&lock));
    rc_rwlock_write_unlock(&lock);

    rc_rwlock_read_lock(&lock);
    rc_rwlock_read_unlock(&lock);
    rc_rwlock_write_lock(&lock);
    rc_rwlock_write_unlock(&lock);
}
