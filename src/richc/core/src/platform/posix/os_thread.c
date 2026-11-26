// os_thread.c
//
// Linux and macOS, with pthreads.

// sysconf's _SC_NPROCESSORS_ONLN is not in POSIX
#define _DEFAULT_SOURCE

#include "os_thread.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

#include "richc/macros.h"


// The OS's entry point, which only changes signature.
static void *rc_os_thread_entry(void *slot)
{
    rc_thread_run_(slot);
    return NULL;
}


uintptr_t rc_os_thread_start(void *slot, uint32_t stack_size)
{
    pthread_attr_t attributes = {0};
    pthread_t thread = {0};
    int result = 0;

    RC_PANIC(pthread_attr_init(&attributes) == 0);
    RC_PANIC(pthread_attr_setstacksize(&attributes, stack_size) == 0);
    result = pthread_create(&thread, &attributes, rc_os_thread_entry, slot);
    (void)pthread_attr_destroy(&attributes);
    if (result != 0) {
        return 0;
    }

    // A pthread_t is an integer on Linux and a pointer on macOS, and never 0 on either
    return (uintptr_t)thread;
}


void rc_os_thread_join(uintptr_t handle)
{
    RC_PANIC(pthread_join((pthread_t)handle, NULL) == 0);
}


void rc_os_thread_yield(void)
{
    (void)sched_yield();
}


void rc_os_thread_sleep_ns(uint64_t ns)
{
    struct timespec left = {0};

    left.tv_sec = (time_t)(ns / 1000000000u);
    left.tv_nsec = (long)(ns % 1000000000u);

    // A signal cuts a sleep short, leaving what remains of it in left
    while (nanosleep(&left, &left) != 0) {
        RC_PANIC(errno == EINTR);
    }
}


uint32_t rc_os_thread_core_count(void)
{
    long count = sysconf(_SC_NPROCESSORS_ONLN);

    return count > 0 ? (uint32_t)count : 1u;
}
