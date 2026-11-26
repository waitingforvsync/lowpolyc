// thread/rwlock.h
//
// A reader-writer lock: any number of readers at once, or one writer. Waiting writers come first,
// so a stream of readers cannot hold a writer off for ever, and a thread must not take a read lock
// it already holds, as a writer may be queued between the two. A zeroed rc_rwlock is unlocked,
// and needs no init or deinit.
//
// An unlock goes on using the lock for a moment after another thread can take it, to wake whoever
// waits. So a thread may free a rwlock only once every thread that used it is done with it, not
// merely once it holds the lock itself; joining them, or a mutex or semaphore they release after
// unlocking, will do.

#ifndef RC_THREAD_RWLOCK_H_
#define RC_THREAD_RWLOCK_H_

#include "richc/thread/atomic.h"

typedef struct rc_rwlock {
    rc_atomic_u32 state_;          // the readers, or all ones for a writer, and two waiting flags
    rc_atomic_u32 writer_wakes_;   // bumped to wake a writer, which sleeps on this
} rc_rwlock;

// Take a read lock if no writer holds the lock or waits for it, returning whether it did.
bool rc_rwlock_try_read_lock(rc_rwlock *lock);

// Take a read lock, sleeping while a writer holds the lock or waits for it.
void rc_rwlock_read_lock(rc_rwlock *lock);

// Release a read lock this thread holds.
void rc_rwlock_read_unlock(rc_rwlock *lock);

// Take the write lock if nobody holds the lock, returning whether it did.
bool rc_rwlock_try_write_lock(rc_rwlock *lock);

// Take the write lock, sleeping while anybody holds the lock.
void rc_rwlock_write_lock(rc_rwlock *lock);

// Release the write lock, which this thread holds.
void rc_rwlock_write_unlock(rc_rwlock *lock);

#endif // RC_THREAD_RWLOCK_H_
