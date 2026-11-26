// futex.c
//
// macOS. os_sync_wait_on_address is the public API, from macOS 14.4. Built for anything older, we
// fall back to __ulock before 14.4, which is private to libSystem but has been there since 10.12,
// and is what libc++'s atomic wait uses. Built for 14.4 or later, __ulock is never referenced, as
// App Store review frowns on private symbols.
//
// Both are capped at the most __ulock takes, UINT32_MAX microseconds (about 71 minutes), as os_sync
// may refuse a much longer timeout. A longer wait stops there and returns early, which callers
// allow for.

#include "platform/futex.h"

#include <errno.h>

#if __has_include(<os/os_sync_wait_on_address.h>)
#  include <os/clock.h>
#  include <os/os_sync_wait_on_address.h>
#  define RC_FUTEX_OS_SYNC_
#  if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 140400
#    define RC_FUTEX_OS_SYNC_ONLY_
#  endif
#endif

// The longest wait either API takes, in nanoseconds.
static const uint64_t rc_futex_longest_ns = (uint64_t)UINT32_MAX * 1000u;

#if !defined(RC_FUTEX_OS_SYNC_ONLY_)

// From xnu's bsd/sys/ulock.h. The wait's timeout is in microseconds, where 0 is forever, and
// ULF_NO_ERRNO returns errors negated instead of setting errno.
extern int __ulock_wait(uint32_t operation, void *addr, uint64_t value, uint32_t timeout);
extern int __ulock_wake(uint32_t operation, void *addr, uint64_t wake_value);

enum {
    RC_UL_COMPARE_AND_WAIT = 1,
    RC_ULF_WAKE_ALL = 0x00000100,
    RC_ULF_NO_ERRNO = 0x01000000
};


// Wait with __ulock, for timeout_ns, which is neither 0 nor over rc_futex_longest_ns.
static bool rc_futex_ulock_wait(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
    // Rounding up keeps a short timeout from becoming 0, which is forever
    uint32_t timeout_us = timeout_ns == RC_FUTEX_FOREVER ? 0 : (uint32_t)((timeout_ns + 999u) / 1000u);
    int result = __ulock_wait(RC_UL_COMPARE_AND_WAIT | RC_ULF_NO_ERRNO, word, expected, timeout_us);

    return result != -ETIMEDOUT;
}

#endif

#if defined(RC_FUTEX_OS_SYNC_)


// Wait with os_sync, for timeout_ns, which is neither 0 nor over rc_futex_longest_ns.
__attribute__((availability(macos, introduced = 14.4)))
static bool rc_futex_os_sync_wait(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
    int result = timeout_ns == RC_FUTEX_FOREVER
        ? os_sync_wait_on_address(word, expected, sizeof(expected), OS_SYNC_WAIT_ON_ADDRESS_NONE)
        : os_sync_wait_on_address_with_timeout(word, expected, sizeof(expected), OS_SYNC_WAIT_ON_ADDRESS_NONE,
            OS_CLOCK_MACH_ABSOLUTE_TIME, timeout_ns);

    return result >= 0 || errno != ETIMEDOUT;
}

#endif


// Wait for timeout_ns, which is neither 0 nor over rc_futex_longest_ns, with whichever API exists.
static bool rc_futex_wait_os(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
#if defined(RC_FUTEX_OS_SYNC_ONLY_)
    return rc_futex_os_sync_wait(word, expected, timeout_ns);
#else
#  if defined(RC_FUTEX_OS_SYNC_)
    if (__builtin_available(macOS 14.4, *)) {
        return rc_futex_os_sync_wait(word, expected, timeout_ns);
    }
#  endif

    return rc_futex_ulock_wait(word, expected, timeout_ns);
#endif
}


// Whether timeout_ns is longer than either API waits, so the wait must stop short.
static bool rc_futex_is_too_long(uint64_t timeout_ns)
{
    return timeout_ns > rc_futex_longest_ns && timeout_ns != RC_FUTEX_FOREVER;
}


bool rc_futex_wait(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
    // Neither API takes a timeout of 0, so that is only a look at the word
    if (timeout_ns == 0) {
        return rc_atomic_u32_load(word, RC_MEMORY_ORDER_RELAXED) != expected;
    }

    // Running out at the limit is not the timeout running out
    if (rc_futex_is_too_long(timeout_ns)) {
        (void)rc_futex_wait_os(word, expected, rc_futex_longest_ns);
        return true;
    }

    return rc_futex_wait_os(word, expected, timeout_ns);
}


bool rc_futex_wake_one(rc_atomic_u32 *word)
{
#if defined(RC_FUTEX_OS_SYNC_ONLY_)
    return os_sync_wake_by_address_any(word, sizeof(uint32_t), OS_SYNC_WAKE_BY_ADDRESS_NONE) == 0;
#else
#  if defined(RC_FUTEX_OS_SYNC_)
    if (__builtin_available(macOS 14.4, *)) {
        return os_sync_wake_by_address_any(word, sizeof(uint32_t), OS_SYNC_WAKE_BY_ADDRESS_NONE) == 0;
    }
#  endif

    // -ENOENT when nobody was waiting
    return __ulock_wake(RC_UL_COMPARE_AND_WAIT | RC_ULF_NO_ERRNO, word, 0) == 0;
#endif
}


void rc_futex_wake_all(rc_atomic_u32 *word)
{
#if defined(RC_FUTEX_OS_SYNC_ONLY_)
    (void)os_sync_wake_by_address_all(word, sizeof(uint32_t), OS_SYNC_WAKE_BY_ADDRESS_NONE);
#else
#  if defined(RC_FUTEX_OS_SYNC_)
    if (__builtin_available(macOS 14.4, *)) {
        (void)os_sync_wake_by_address_all(word, sizeof(uint32_t), OS_SYNC_WAKE_BY_ADDRESS_NONE);
        return;
    }
#  endif

    (void)__ulock_wake(RC_UL_COMPARE_AND_WAIT | RC_ULF_WAKE_ALL | RC_ULF_NO_ERRNO, word, 0);
#endif
}
