// os_thread.c
//
// Windows, with CreateThread.

#include "platform/os_thread.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "richc/time.h"


// The OS's entry point, which only changes signature. The thread's slot index comes as the pointer.
static DWORD WINAPI rc_os_thread_entry(LPVOID index)
{
    rc_thread_run_((uint32_t)(uintptr_t)index);
    return 0;
}


uintptr_t rc_os_thread_start(uint32_t index, uint32_t stack_size)
{
    // Without the flag the size is only what is committed at first, and the reserve is the exe's
    HANDLE thread = CreateThread(NULL, stack_size, rc_os_thread_entry, (LPVOID)(uintptr_t)index, STACK_SIZE_PARAM_IS_A_RESERVATION,
        NULL);

    return (uintptr_t)thread;
}


void rc_os_thread_join(uintptr_t handle)
{
    RC_PANIC(WaitForSingleObject((HANDLE)handle, INFINITE) == WAIT_OBJECT_0);
    RC_PANIC(CloseHandle((HANDLE)handle));
}


void rc_os_thread_yield(void)
{
    (void)SwitchToThread();
}


void rc_os_thread_sleep_ns(uint64_t ns)
{
    uint64_t start = rc_time_now_ns();

    // Sleep counts milliseconds, rounded up here, but follows the clock tick, so it can still end
    // a little early by the monotonic clock; sleep again for whatever is left
    for (uint64_t elapsed = 0; elapsed < ns; elapsed = rc_time_now_ns() - start) {
        uint64_t left = ns - elapsed;
        uint64_t ms = left / 1000000u + (left % 1000000u != 0 ? 1u : 0u);

        Sleep(ms < INFINITE ? (DWORD)ms : INFINITE - 1);
    }
}


uint32_t rc_os_thread_core_count(void)
{
    // Every processor group, where GetSystemInfo sees only this thread's, at most 64 cores
    DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

    return count > 0 ? (uint32_t)count : 1u;
}
