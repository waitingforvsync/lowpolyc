// futex.c
//
// wasm32, with one thread. Nothing else can change the word or wake a sleeper, so a wait with a
// timeout passes the time, and a wait without one would never end.

#include "futex.h"

#include "richc/time.h"


bool rc_futex_wait(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
    uint64_t start = rc_time_now_ns();

    if (rc_atomic_u32_load(word, RC_MEMORY_ORDER_RELAXED) != expected) {
        return true;
    }

    RC_PANIC(timeout_ns != RC_FUTEX_FOREVER);
    while (rc_time_now_ns() - start < timeout_ns) {
        // There is no sleep to call, and the page is stuck here either way
    }

    return false;
}


bool rc_futex_wake_one(rc_atomic_u32 *word)
{
    (void)word;
    return false;
}


void rc_futex_wake_all(rc_atomic_u32 *word)
{
    (void)word;
}
