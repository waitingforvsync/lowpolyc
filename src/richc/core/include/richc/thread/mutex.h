// thread/mutex.h
//
// A lock whose waiters sleep, for critical sections of any length. It spins briefly first, as
// most are short. A zeroed rc_mutex is unlocked, and needs no init or deinit. It is not
// recursive: a thread must not lock one it already holds.

#ifndef RC_THREAD_MUTEX_H_
#define RC_THREAD_MUTEX_H_

#include "richc/thread/atomic.h"

typedef struct rc_mutex {
    rc_atomic_u32 state_;   // 0 unlocked, 1 locked, 2 locked with threads perhaps asleep
} rc_mutex;

// Take the mutex if it is free, returning whether it was.
bool rc_mutex_try_lock(rc_mutex *mutex);

// Take the mutex, sleeping until it is free.
void rc_mutex_lock(rc_mutex *mutex);

// Release the mutex, which this thread holds.
void rc_mutex_unlock(rc_mutex *mutex);

#endif // RC_THREAD_MUTEX_H_
