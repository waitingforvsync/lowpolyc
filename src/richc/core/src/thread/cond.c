// cond.c
//
// The condition variable, over a futex on a counter that every signal bumps. A waiter reads the
// counter while it still holds the mutex, so a signal sent after it checked its condition, which
// took the mutex to change that condition, changes the counter too, and the futex will not sleep
// through it.
//
// Waiters count themselves under the mutex too, before they release it, so a signal sent after
// the condition changed sees every waiter that could need it, and with none skips the wake and
// its syscall.

#include "richc/thread/cond.h"

#include "platform/futex.h"


// Stop counting as a waiter and take mutex back after a wait, passing on what the wait returned.
// The thread looks at its condition again, under the mutex, before it could need another wake, so
// it stops counting at once rather than once it has the mutex, and signals sent while it waits for
// the mutex skip their syscalls.
static bool rc_cond_relock(rc_cond *cond, rc_mutex *mutex, bool woken)
{
    (void)rc_atomic_u32_fetch_sub(&cond->waiters_, 1, RC_MEMORY_ORDER_RELAXED);
    rc_mutex_lock(mutex);
    return woken;
}


// Wait for a signal or timeout_ns, returning false if the time ran out.
static bool rc_cond_sleep(rc_cond *cond, rc_mutex *mutex, uint64_t timeout_ns)
{
    uint32_t seq = rc_atomic_u32_load(&cond->seq_, RC_MEMORY_ORDER_RELAXED);

    (void)rc_atomic_u32_fetch_add(&cond->waiters_, 1, RC_MEMORY_ORDER_RELAXED);
    rc_mutex_unlock(mutex);
    return rc_cond_relock(cond, mutex, rc_futex_wait(&cond->seq_, seq, timeout_ns));
}


void rc_cond_wait(rc_cond *cond, rc_mutex *mutex)
{
    (void)rc_cond_sleep(cond, mutex, RC_FUTEX_FOREVER);
}


bool rc_cond_wait_for(rc_cond *cond, rc_mutex *mutex, uint64_t timeout_ns)
{
    return rc_cond_sleep(cond, mutex, timeout_ns);
}


void rc_cond_signal(rc_cond *cond)
{
    if (rc_atomic_u32_load(&cond->waiters_, RC_MEMORY_ORDER_RELAXED) == 0) {
        return;
    }

    (void)rc_atomic_u32_fetch_add(&cond->seq_, 1, RC_MEMORY_ORDER_RELAXED);
    (void)rc_futex_wake_one(&cond->seq_);
}


void rc_cond_broadcast(rc_cond *cond)
{
    if (rc_atomic_u32_load(&cond->waiters_, RC_MEMORY_ORDER_RELAXED) == 0) {
        return;
    }

    (void)rc_atomic_u32_fetch_add(&cond->seq_, 1, RC_MEMORY_ORDER_RELAXED);
    rc_futex_wake_all(&cond->seq_);
}
