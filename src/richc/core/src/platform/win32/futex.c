// futex.c
//
// Windows, with WaitOnAddress from Windows 8 on, in Synchronization.lib.

#include "futex.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


bool rc_futex_wait(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
    DWORD timeout_ms = INFINITE;

    // Rounding up keeps a short timeout from becoming 0. A timeout too long to fit returns early,
    // which callers allow for.
    if (timeout_ns != RC_FUTEX_FOREVER) {
        uint64_t rounded = timeout_ns / 1000000u + (timeout_ns % 1000000u != 0);

        timeout_ms = rounded < INFINITE ? (DWORD)rounded : INFINITE - 1;
    }

    if (WaitOnAddress(word, &expected, sizeof(expected), timeout_ms)) {
        return true;
    }

    return GetLastError() != ERROR_TIMEOUT;
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
