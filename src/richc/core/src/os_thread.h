// os_thread.h
//
// The OS's threads, under thread/thread.c, which keeps the table of them. Private to core, and
// implemented once per target in platform/<target>/os_thread.c: pthreads on Linux and macOS,
// CreateThread on Windows, and on wasm, with one thread, nothing to start.

#ifndef RC_OS_THREAD_H_
#define RC_OS_THREAD_H_

#include <stdbool.h>
#include <stdint.h>

// Start an OS thread with a stack of stack_size bytes, running rc_thread_run_(slot). Returns its
// handle, or 0 if the OS would not start one.
uintptr_t rc_os_thread_start(void *slot, uint32_t stack_size);

// Wait for the thread with handle to finish, and free what the OS keeps for it.
void rc_os_thread_join(uintptr_t handle);

// Give the rest of this thread's time slice away.
void rc_os_thread_yield(void);

// Sleep for at least ns nanoseconds.
void rc_os_thread_sleep_ns(uint64_t ns);

// The machine's logical cores, at least 1.
uint32_t rc_os_thread_core_count(void);

// What every thread started by rc_os_thread_start runs, in thread/thread.c.
void rc_thread_run_(void *slot);

#endif // RC_OS_THREAD_H_
