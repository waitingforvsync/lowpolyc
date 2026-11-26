// semaphore.c
//
// The semaphore, over a futex on its count, which a sleeper waits on while it is 0. A post makes
// a syscall only when sleepers_ says somebody may be asleep. Each side writes its own word, then
// a seq_cst fence, then reads the other's, so either the post sees the sleeper or the sleeper's
// futex sees the post's count.

#include "richc/thread/semaphore.h"

#include "platform/futex.h"
#include "richc/thread/spinlock.h"
#include "richc/time.h"


// Sleep until the count may have risen or timeout_ns passes, returning false if the time ran out.
static bool rc_semaphore_sleep(rc_semaphore *semaphore, uint64_t timeout_ns)
{
    bool woken = false;

    (void)rc_atomic_u32_fetch_add(&semaphore->sleepers_, 1, RC_MEMORY_ORDER_RELAXED);
    rc_atomic_thread_fence(RC_MEMORY_ORDER_SEQ_CST);
    woken = rc_futex_wait(&semaphore->count_, 0, timeout_ns);
    (void)rc_atomic_u32_fetch_sub(&semaphore->sleepers_, 1, RC_MEMORY_ORDER_RELAXED);
    return woken;
}


rc_semaphore rc_semaphore_make(uint32_t count)
{
    rc_semaphore semaphore = {0};

    semaphore.count_ = rc_atomic_u32_make(count);
    return semaphore;
}


bool rc_semaphore_try_wait(rc_semaphore *semaphore)
{
    uint32_t count = rc_atomic_u32_load(&semaphore->count_, RC_MEMORY_ORDER_RELAXED);

    while (count != 0) {
        uint32_t found = rc_atomic_u32_compare_exchange(&semaphore->count_, count, count - 1, RC_MEMORY_ORDER_ACQUIRE);

        if (found == count) {
            return true;
        }
        count = found;
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
    for (;;) {
        uint64_t elapsed = 0;

        if (rc_semaphore_try_wait(semaphore)) {
            return true;
        }

        elapsed = rc_time_now_ns() - start;
        if (elapsed >= timeout_ns) {
            return false;
        }

        (void)rc_semaphore_sleep(semaphore, timeout_ns - elapsed);
    }
}


void rc_semaphore_post(rc_semaphore *semaphore, uint32_t count)
{
    uint32_t before = rc_atomic_u32_fetch_add(&semaphore->count_, count, RC_MEMORY_ORDER_RELEASE);

    RC_ASSERT(before + count >= before);
    rc_atomic_thread_fence(RC_MEMORY_ORDER_SEQ_CST);
    if (rc_atomic_u32_load(&semaphore->sleepers_, RC_MEMORY_ORDER_RELAXED) == 0) {
        return;
    }

    if (count == 1) {
        (void)rc_futex_wake_one(&semaphore->count_);
    } else {
        rc_futex_wake_all(&semaphore->count_);
    }
}
