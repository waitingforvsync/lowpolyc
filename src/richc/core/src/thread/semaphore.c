// semaphore.c
//
// The semaphore, after glibc's: one 64-bit word holds the count and how many threads sleep, and
// sleepers wait on the count's half of it. A sleeper counts itself and a post adds to the count
// with an atomic add to the same word, so whichever is first, the other sees it: the post sees the
// sleeper and wakes it, or the sleeper's futex sees the count and does not sleep. A post with no
// sleepers makes no syscall, and after its add it uses only the semaphore's address, for the wake,
// so a waiter may free the semaphore as soon as it has the count.

#include "richc/thread/semaphore.h"

#include "platform/futex.h"
#include "richc/thread/spinlock.h"
#include "richc/time.h"

// One sleeper, in the high half of the state.
#define RC_SEMAPHORE_SLEEPER ((uint64_t)1 << 32)


// Stop counting as a sleeper, passing on what the wait returned.
static bool rc_semaphore_wake_up(rc_semaphore *semaphore, bool woken)
{
    (void)rc_atomic_u64_fetch_sub(&semaphore->state_, RC_SEMAPHORE_SLEEPER, RC_MEMORY_ORDER_RELAXED);
    return woken;
}


// Sleep until the count may have risen or timeout_ns passes, returning false if the time ran out.
static bool rc_semaphore_sleep(rc_semaphore *semaphore, uint64_t timeout_ns)
{
    (void)rc_atomic_u64_fetch_add(&semaphore->state_, RC_SEMAPHORE_SLEEPER, RC_MEMORY_ORDER_RELAXED);
    return rc_semaphore_wake_up(semaphore, rc_futex_wait(&semaphore->count_, 0, timeout_ns));
}


rc_semaphore rc_semaphore_make(uint32_t count)
{
    return (rc_semaphore) {.state_ = rc_atomic_u64_make(count)};
}


bool rc_semaphore_try_wait(rc_semaphore *semaphore)
{
    uint64_t state = rc_atomic_u64_load(&semaphore->state_, RC_MEMORY_ORDER_RELAXED);

    while ((uint32_t)state != 0) {
        uint64_t found = rc_atomic_u64_compare_exchange(&semaphore->state_, state, state - 1, RC_MEMORY_ORDER_ACQUIRE);

        if (found == state) {
            return true;
        }

        state = found;
    }

    return false;
}


void rc_semaphore_wait(rc_semaphore *semaphore)
{
    // A post is often only moments away, and cheaper to catch awake
    for (uint32_t i = 0; i < RC_FUTEX_SPINS; i++) {
        if (rc_semaphore_try_wait(semaphore)) {
            return;
        }

        rc_spin_pause();
    }

    while (!rc_semaphore_try_wait(semaphore)) {
        (void)rc_semaphore_sleep(semaphore, RC_FUTEX_FOREVER);
    }
}


bool rc_semaphore_wait_for(rc_semaphore *semaphore, uint64_t timeout_ns)
{
    uint64_t start = rc_time_now_ns();

    // Another thread can take the count between a wake and our try, so each sleep gets only what
    // is left of the time
    while (!rc_semaphore_try_wait(semaphore)) {
        uint64_t elapsed = rc_time_now_ns() - start;

        if (elapsed >= timeout_ns) {
            return false;
        }

        (void)rc_semaphore_sleep(semaphore, timeout_ns - elapsed);
    }

    return true;
}


void rc_semaphore_post(rc_semaphore *semaphore, uint32_t count)
{
    uint64_t before = rc_atomic_u64_fetch_add(&semaphore->state_, count, RC_MEMORY_ORDER_RELEASE);

    // The count must not carry into the sleepers
    RC_ASSERT((uint32_t)before + count >= (uint32_t)before);
    if ((before >> 32) == 0) {
        return;
    }

    if (count == 0) {
        return;
    }

    if (count == 1) {
        (void)rc_futex_wake_one(&semaphore->count_);
    } else {
        rc_futex_wake_all(&semaphore->count_);
    }
}
