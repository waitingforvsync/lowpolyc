// thread/spinlock.h
//
// A lock that waits by spinning, never sleeping, for critical sections only a few instructions
// long. A waiter burns its core until the lock is free, so never hold one across anything that
// blocks or takes a while. A zeroed rc_spinlock is unlocked.

#ifndef RC_THREAD_SPINLOCK_H_
#define RC_THREAD_SPINLOCK_H_

#include "richc/thread/atomic.h"

typedef struct rc_spinlock {
    rc_atomic_bool locked_;
} rc_spinlock;


// Tell the core this thread is spinning, which on x86 and ARM saves power and gives the core's
// other hardware thread a turn. It does nothing on wasm.
static inline void rc_spin_pause(void)
{
#if defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
    _mm_pause();
#elif defined(_MSC_VER) && !defined(__clang__)
    __yield();
#elif defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield");
#endif
}


// Take the lock if it is free, returning whether it was.
static inline bool rc_spinlock_try_lock(rc_spinlock *lock)
{
    // Looking first keeps a waiter reading its own cached copy, instead of writing to the line the
    // holder needs, until the lock comes free
    if (rc_atomic_bool_load(&lock->locked_, RC_MEMORY_ORDER_RELAXED)) {
        return false;
    }

    return !rc_atomic_bool_exchange(&lock->locked_, true, RC_MEMORY_ORDER_ACQUIRE);
}


// Take the lock, spinning until it is free.
static inline void rc_spinlock_lock(rc_spinlock *lock)
{
    while (!rc_spinlock_try_lock(lock)) {
        rc_spin_pause();
    }
}


// Release the lock, which this thread holds.
static inline void rc_spinlock_unlock(rc_spinlock *lock)
{
    RC_ASSERT(rc_atomic_bool_load(&lock->locked_, RC_MEMORY_ORDER_RELAXED));
    rc_atomic_bool_store(&lock->locked_, false, RC_MEMORY_ORDER_RELEASE);
}

#endif // RC_THREAD_SPINLOCK_H_
