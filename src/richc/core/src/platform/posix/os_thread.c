// os_thread.c
//
// Linux and macOS, with pthreads.

// sysconf's _SC_NPROCESSORS_ONLN is not in POSIX
#define _DEFAULT_SOURCE

#include "platform/os_thread.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

#include "richc/macros.h"


// The OS's entry point, which only changes signature. The thread's slot index comes as the pointer.
static void *rc_os_thread_entry(void *index)
{
    rc_thread_run_((uint32_t)(uintptr_t)index);
    return NULL;
}


// Start the thread for slot index with attributes, newly made, which this then destroys. Returns
// its handle, or 0 if it did not start.
static uintptr_t rc_os_thread_create(pthread_attr_t *attributes, uint32_t index, uint32_t stack_size)
{
    pthread_t thread = {0};
    int sized = pthread_attr_setstacksize(attributes, stack_size);
    int created = sized == 0 ? pthread_create(&thread, attributes, rc_os_thread_entry, (void *)(uintptr_t)index) : sized;

    (void)pthread_attr_destroy(attributes);

    // A pthread_t is an integer on Linux and a pointer on macOS, and never 0 on either
    return created == 0 ? (uintptr_t)thread : 0;
}


uintptr_t rc_os_thread_start(uint32_t index, uint32_t stack_size)
{
    pthread_attr_t attributes = {0};

    if (pthread_attr_init(&attributes) != 0) {
        return 0;
    }

    return rc_os_thread_create(&attributes, index, stack_size);
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
    struct timespec left = {
        .tv_sec = (time_t)(ns / 1000000000u),
        .tv_nsec = (long)(ns % 1000000000u)
    };

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
