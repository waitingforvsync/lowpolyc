// futex.c
//
// Linux, with the futex syscall. The private operations skip the kernel's work for words shared
// between processes.

// syscall is not in POSIX
#define _DEFAULT_SOURCE

#include "platform/futex.h"

#include <errno.h>
#include <limits.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>


// timeout_ns as the kernel takes it.
static struct timespec rc_futex_timespec(uint64_t timeout_ns)
{
    return (struct timespec) {
        .tv_sec = (time_t)(timeout_ns / 1000000000u),
        .tv_nsec = (long)(timeout_ns % 1000000000u)
    };
}


bool rc_futex_wait(rc_atomic_u32 *word, uint32_t expected, uint64_t timeout_ns)
{
    struct timespec timeout = rc_futex_timespec(timeout_ns);
    const struct timespec *limit = timeout_ns != RC_FUTEX_FOREVER ? &timeout : NULL;
    int64_t result = syscall(SYS_futex, word, FUTEX_WAIT_PRIVATE, expected, limit, NULL, 0);

    // EAGAIN (the word had already changed) and EINTR (a signal) are early returns like any other
    return result == 0 || errno != ETIMEDOUT;
}


bool rc_futex_wake_one(rc_atomic_u32 *word)
{
    return syscall(SYS_futex, word, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0) > 0;
}


void rc_futex_wake_all(rc_atomic_u32 *word)
{
    (void)syscall(SYS_futex, word, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
}
