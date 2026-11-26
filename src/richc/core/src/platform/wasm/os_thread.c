// os_thread.c
//
// wasm32, with one thread, so no thread can start and there is nothing to yield to. A sleep can
// only pass the time.

#include "os_thread.h"

#include "richc/macros.h"
#include "richc/time.h"


uintptr_t rc_os_thread_start(void *slot, uint32_t stack_size)
{
    (void)slot;
    (void)stack_size;
    return 0;
}


void rc_os_thread_join(uintptr_t handle)
{
    // Nothing ever started, so nothing can be joined
    (void)handle;
    RC_PANIC(false);
}


void rc_os_thread_yield(void)
{
}


void rc_os_thread_sleep_ns(uint64_t ns)
{
    uint64_t start = rc_time_now_ns();

    while (rc_time_now_ns() - start < ns) {
        // There is no sleep to call, and the page is stuck here either way
    }
}


uint32_t rc_os_thread_core_count(void)
{
    return 1;
}
