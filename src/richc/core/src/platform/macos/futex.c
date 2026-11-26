// futex.c
//
// macOS. os_sync_wait_on_address is the public API, from macOS 14.4. Before that we fall back to
// __ulock, which is private to libSystem but has been there since 10.12, and is what libc++'s
// atomic wait uses.

#include "platform/futex.h"

#include <errno.h>

#if __has_include(<os/os_sync_wait_on_address.h>)
#  include <os/clock.h>
#  include <os/os_sync_wait_on_address.h>
#  define RC_FUTEX_OS_SYNC_
#endif

// From xnu's bsd/sys/ulock.h. The wait's timeout is in microseconds, where 0 is forever, and
// ULF_NO_ERRNO returns errors negated instead of setting errno.
extern int __ulock_wait(uint32_t operation, void *addr, uint64_t value, uint32_t timeout);
extern int __ulock_wake(uint32_t operation, void *addr, uint64_t wake_value);

enum {
    RC_UL_COMPARE_AND_WAIT = 1,
    RC_ULF_WAKE_ALL = 0x00000100,
    RC_ULF_NO_ERRNO = 0x01000000
};


// Wait with __ulock, for timeout_ns (which is not 0).
static bool rc_futex_ulock_wait(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
    uint64_t timeout_us = 0;
    int result = 0;

    // Rounding up keeps a short timeout from becoming 0, which is forever. A timeout too long to
    // fit returns early, which callers allow for.
    if (timeout_ns != RC_FUTEX_FOREVER) {
        timeout_us = timeout_ns / 1000u + (timeout_ns % 1000u != 0);
        timeout_us = timeout_us < UINT32_MAX ? timeout_us : UINT32_MAX;
    }

    result = __ulock_wait(RC_UL_COMPARE_AND_WAIT | RC_ULF_NO_ERRNO, word, expected, (uint32_t)timeout_us);
    return result != -ETIMEDOUT;
}


bool rc_futex_wait(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
    // Neither API takes a timeout of 0, so that is only a look at the word
    if (timeout_ns == 0) {
        return rc_atomic_u32_load(word, RC_MEMORY_ORDER_RELAXED) != expected;
    }

#if defined(RC_FUTEX_OS_SYNC_)
    if (__builtin_available(macOS 14.4, *)) {
        int result = 0;

        if (timeout_ns == RC_FUTEX_FOREVER) {
            result = os_sync_wait_on_address(word, expected, sizeof(expected), OS_SYNC_WAIT_ON_ADDRESS_NONE);
        } else {
            result = os_sync_wait_on_address_with_timeout(word, expected, sizeof(expected), OS_SYNC_WAIT_ON_ADDRESS_NONE,
                OS_CLOCK_MACH_ABSOLUTE_TIME, timeout_ns);
        }

        return result >= 0 || errno != ETIMEDOUT;
    }
#endif

    return rc_futex_ulock_wait(word, expected, timeout_ns);
}


bool rc_futex_wake_one(rc_atomic_u32 *word)
{
#if defined(RC_FUTEX_OS_SYNC_)
    if (__builtin_available(macOS 14.4, *)) {
        return os_sync_wake_by_address_any(word, sizeof(uint32_t), OS_SYNC_WAKE_BY_ADDRESS_NONE) == 0;
    }
#endif

    // -ENOENT when nobody was waiting
    return __ulock_wake(RC_UL_COMPARE_AND_WAIT | RC_ULF_NO_ERRNO, word, 0) == 0;
}


void rc_futex_wake_all(rc_atomic_u32 *word)
{
#if defined(RC_FUTEX_OS_SYNC_)
    if (__builtin_available(macOS 14.4, *)) {
        (void)os_sync_wake_by_address_all(word, sizeof(uint32_t), OS_SYNC_WAKE_BY_ADDRESS_NONE);
        return;
    }
#endif

    (void)__ulock_wake(RC_UL_COMPARE_AND_WAIT | RC_ULF_WAKE_ALL | RC_ULF_NO_ERRNO, word, 0);
}
