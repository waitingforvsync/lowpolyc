// cond.c
//
// The condition variable, over a futex on a counter that every signal bumps. A waiter reads the
// counter while it still holds the mutex, so a signal sent after it checked its condition, which
// took the mutex to change that condition, changes the counter too, and the futex will not sleep
// through it.

#include "richc/thread/cond.h"

#include "platform/futex.h"


// Wait for a signal or timeout_ns, returning false if the time ran out.
static bool rc_cond_sleep(rc_cond *cond, rc_mutex *mutex, uint64_t timeout_ns)
{
    uint32_t seq = rc_atomic_u32_load(&cond->seq_, RC_MEMORY_ORDER_RELAXED);
    bool woken = false;

    rc_mutex_unlock(mutex);
    woken = rc_futex_wait(&cond->seq_, seq, timeout_ns);
    rc_mutex_lock(mutex);
    return woken;
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
    (void)rc_atomic_u32_fetch_add(&cond->seq_, 1, RC_MEMORY_ORDER_RELAXED);
    (void)rc_futex_wake_one(&cond->seq_);
}


void rc_cond_broadcast(rc_cond *cond)
{
    (void)rc_atomic_u32_fetch_add(&cond->seq_, 1, RC_MEMORY_ORDER_RELAXED);
    rc_futex_wake_all(&cond->seq_);
}
