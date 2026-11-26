// futex.c
//
// Windows, with WaitOnAddress from Windows 8 on, in Synchronization.lib. It counts milliseconds,
// up to INFINITE - 1 (about 49.7 days), so a longer wait stops there and returns early, which
// callers allow for.

#include "platform/futex.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// The longest wait WaitOnAddress takes, in nanoseconds.
static const uint64_t rc_futex_longest_ns = (uint64_t)(INFINITE - 1) * 1000000u;


// Wait for timeout_ns, which is not over rc_futex_longest_ns.
static bool rc_futex_wait_os(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
    // Rounding up keeps a short timeout from becoming 0
    DWORD timeout_ms = timeout_ns == RC_FUTEX_FOREVER ? INFINITE : (DWORD)((timeout_ns + 999999u) / 1000000u);

    if (WaitOnAddress(word, &expected, sizeof(expected), timeout_ms)) {
        return true;
    }

    return GetLastError() != ERROR_TIMEOUT;
}


// Whether timeout_ns is longer than WaitOnAddress waits, so the wait must stop short.
static bool rc_futex_is_too_long(uint64_t timeout_ns)
{
    return timeout_ns > rc_futex_longest_ns && timeout_ns != RC_FUTEX_FOREVER;
}


bool rc_futex_wait(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
    // Running out at the limit is not the timeout running out
    if (rc_futex_is_too_long(timeout_ns)) {
        (void)rc_futex_wait_os(word, expected, rc_futex_longest_ns);
        return true;
    }

    return rc_futex_wait_os(word, expected, timeout_ns);
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
