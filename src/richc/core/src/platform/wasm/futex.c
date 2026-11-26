// futex.c
//
// wasm32. With threads, memory.atomic.wait32 and notify, except on a page's main thread, which
// may not wait and spins instead: anything it waits for comes from a worker, which writes the
// word before waking anyone, so a spinner sees the change without being woken.
//
// Without threads, nothing else can change the word or wake a sleeper, so a wait with a timeout
// passes the time, and a wait without one would never end.

#include "futex.h"

#include "richc/time.h"

#if defined(__wasm_atomics__)

#include "richc/thread/spinlock.h"
#include "wasm_thread.h"


// Wait by looking at the word until it changes or the time runs out.
static bool rc_futex_spin(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
    uint64_t start = rc_time_now_ns();

    while (rc_atomic_u32_load(word, RC_MEMORY_ORDER_RELAXED) == expected) {
        if (timeout_ns != RC_FUTEX_FOREVER && rc_time_now_ns() - start >= timeout_ns) {
            return false;
        }
        rc_spin_pause();
    }

    return true;
}


bool rc_futex_wait(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
    int64_t timeout = -1;

    if (!rc_wasm_thread_may_block()) {
        return rc_futex_spin(word, expected, timeout_ns);
    }

    // A negative timeout is forever, and one too long to fit nearly is
    if (timeout_ns < (uint64_t)INT64_MAX) {
        timeout = (int64_t)timeout_ns;
    }

    // 0 woken, 1 the word had changed already, 2 timed out
    return __builtin_wasm_memory_atomic_wait32((int32_t *)word, (int32_t)expected, timeout) != 2;
}


bool rc_futex_wake_one(rc_atomic_u32 *word)
{
    // A spinning main thread is not counted, so 0 is "perhaps nobody", as the header allows
    return __builtin_wasm_memory_atomic_notify((int32_t *)word, 1) > 0;
}


void rc_futex_wake_all(rc_atomic_u32 *word)
{
    (void)__builtin_wasm_memory_atomic_notify((int32_t *)word, UINT32_MAX);
}

#else


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

#endif
