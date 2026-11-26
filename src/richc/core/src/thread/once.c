// once.c
//
// rc_once, over a futex on its state. The thread that moves it from not run to running calls the
// function; any other that finds it running marks it as having sleepers and sleeps, and the
// runner wakes them all when it marks it done.

#include "richc/thread/once.h"

#include "platform/futex.h"

enum {
    RC_ONCE_NOT_RUN = 0,
    RC_ONCE_RUNNING = 1,
    RC_ONCE_SLEEPERS = 2,
    RC_ONCE_DONE = 3
};


void rc_once_call(rc_once *once, void (*fn)(void *), void *context)
{
    // Acquire, so whatever fn wrote is visible once done is
    uint32_t state = rc_atomic_u32_load(&once->state_, RC_MEMORY_ORDER_ACQUIRE);

    while (state != RC_ONCE_DONE) {
        if (state == RC_ONCE_NOT_RUN) {
            state = rc_atomic_u32_compare_exchange(&once->state_, RC_ONCE_NOT_RUN, RC_ONCE_RUNNING, RC_MEMORY_ORDER_ACQUIRE);
            if (state != RC_ONCE_NOT_RUN) {
                continue;
            }

            fn(context);
            if (rc_atomic_u32_exchange(&once->state_, RC_ONCE_DONE, RC_MEMORY_ORDER_RELEASE) == RC_ONCE_SLEEPERS) {
                rc_futex_wake_all(&once->state_);
            }
            return;
        }

        if (state == RC_ONCE_RUNNING) {
            state = rc_atomic_u32_compare_exchange(&once->state_, RC_ONCE_RUNNING, RC_ONCE_SLEEPERS, RC_MEMORY_ORDER_ACQUIRE);
            if (state != RC_ONCE_RUNNING) {
                continue;
            }
        }

        (void)rc_futex_wait(&once->state_, RC_ONCE_SLEEPERS, RC_FUTEX_FOREVER);
        state = rc_atomic_u32_load(&once->state_, RC_MEMORY_ORDER_ACQUIRE);
    }
}
