// mutex.c
//
// The mutex, over a futex on its state, after Drepper's "Futexes are tricky". Only a thread that
// has had to sleep marks the state 2, and it keeps 2 when it gets the lock, as others may still be
// asleep, so unlocking from 2 always wakes one. An uncontended lock and unlock make no syscall.

#include "richc/thread/mutex.h"

#include "platform/futex.h"
#include "richc/thread/spinlock.h"

enum {
    RC_MUTEX_UNLOCKED = 0,
    RC_MUTEX_LOCKED = 1,
    RC_MUTEX_SLEEPERS = 2
};


// Spin while the mutex is held with nobody asleep, as it is likely to come free soon, returning
// the state once it is anything else or spinning has gone on long enough.
static uint32_t rc_mutex_spin(rc_mutex *mutex)
{
    uint32_t state = rc_atomic_u32_load(&mutex->state_, RC_MEMORY_ORDER_RELAXED);

    for (uint32_t i = 0; i < RC_FUTEX_SPINS && state == RC_MUTEX_LOCKED; i++) {
        rc_spin_pause();
        state = rc_atomic_u32_load(&mutex->state_, RC_MEMORY_ORDER_RELAXED);
    }

    return state;
}


// Take the mutex once the quick attempt has found it held.
static void rc_mutex_lock_contended(rc_mutex *mutex)
{
    uint32_t state = rc_mutex_spin(mutex);

    // Free now, and nobody was asleep, so there will be nobody to wake
    if (state == RC_MUTEX_UNLOCKED) {
        state = rc_atomic_u32_compare_exchange(&mutex->state_, RC_MUTEX_UNLOCKED, RC_MUTEX_LOCKED, RC_MEMORY_ORDER_ACQUIRE);
        if (state == RC_MUTEX_UNLOCKED) {
            return;
        }
    }

    // Swapping in 2 tells the holder to wake us, and takes the mutex if it came free in between
    for (;;) {
        if (state != RC_MUTEX_SLEEPERS) {
            if (rc_atomic_u32_exchange(&mutex->state_, RC_MUTEX_SLEEPERS, RC_MEMORY_ORDER_ACQUIRE) == RC_MUTEX_UNLOCKED) {
                return;
            }
        }

        (void)rc_futex_wait(&mutex->state_, RC_MUTEX_SLEEPERS, RC_FUTEX_FOREVER);
        state = rc_mutex_spin(mutex);
    }
}


bool rc_mutex_try_lock(rc_mutex *mutex)
{
    return rc_atomic_u32_compare_exchange(&mutex->state_, RC_MUTEX_UNLOCKED, RC_MUTEX_LOCKED, RC_MEMORY_ORDER_ACQUIRE)
        == RC_MUTEX_UNLOCKED;
}


void rc_mutex_lock(rc_mutex *mutex)
{
    if (!rc_mutex_try_lock(mutex)) {
        rc_mutex_lock_contended(mutex);
    }
}


void rc_mutex_unlock(rc_mutex *mutex)
{
    uint32_t state = rc_atomic_u32_exchange(&mutex->state_, RC_MUTEX_UNLOCKED, RC_MEMORY_ORDER_RELEASE);

    RC_ASSERT(state != RC_MUTEX_UNLOCKED);
    if (state == RC_MUTEX_SLEEPERS) {
        (void)rc_futex_wake_one(&mutex->state_);
    }
}
