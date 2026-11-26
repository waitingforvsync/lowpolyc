// futex.c
//
// Windows, with WaitOnAddress from Windows 8 on, in Synchronization.lib. It counts milliseconds,
// up to INFINITE - 1 (about 49.7 days), so a longer wait stops there and returns early, which
// callers allow for. Its timeouts follow the system's clock tick, and can run out a little before
// the monotonic clock says the time has passed, so a timeout counts only once that clock agrees.

#include "platform/futex.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "richc/time.h"

// The longest wait WaitOnAddress takes, in nanoseconds.
static const uint64_t rc_futex_longest_ns = (uint64_t)(INFINITE - 1) * 1000000u;


// Wait once for timeout_ns, which is not over rc_futex_longest_ns, returning false if Windows says
// it ran out.
static bool rc_futex_wait_os(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
    // Rounding up keeps a short timeout from becoming 0
    DWORD timeout_ms = timeout_ns == RC_FUTEX_FOREVER ? INFINITE : (DWORD)((timeout_ns + 999999u) / 1000000u);

    if (WaitOnAddress(word, &expected, sizeof(expected), timeout_ms)) {
        return true;
    }

    return GetLastError() != ERROR_TIMEOUT;
}


// Wait for timeout_ns by the monotonic clock, waiting again for what is left whenever Windows
// runs out early.
static bool rc_futex_wait_timed(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
    uint64_t start = rc_time_now_ns();

    for (uint64_t elapsed = 0; elapsed < timeout_ns; elapsed = rc_time_now_ns() - start) {
        uint64_t left = timeout_ns - elapsed;

        // Running out at the limit is not the timeout running out
        if (left > rc_futex_longest_ns) {
            (void)rc_futex_wait_os(word, expected, rc_futex_longest_ns);
            return true;
        }

        if (rc_futex_wait_os(word, expected, left)) {
            return true;
        }
    }

    return false;
}


bool rc_futex_wait(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
    // A timeout of 0 is only a look at the word
    if (timeout_ns == 0) {
        return rc_atomic_u32_load(word, RC_MEMORY_ORDER_RELAXED) != expected;
    }

    if (timeout_ns == RC_FUTEX_FOREVER) {
        return rc_futex_wait_os(word, expected, RC_FUTEX_FOREVER);
    }

    return rc_futex_wait_timed(word, expected, timeout_ns);
}


bool rc_futex_wake_one(rc_atomic_u32 *word)
{
    // Windows does not say whether anyone was waiting
    WakeByAddressSingle(word);
    return false;
}


void rc_futex_wake_all(rc_atomic_u32 *word)
{
    WakeByAddressAll(word);
}
