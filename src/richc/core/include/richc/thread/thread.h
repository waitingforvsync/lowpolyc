// thread/thread.h
//
// Starting and joining threads, and a few things about the thread running now. A thread is a
// small handle by value, naming a slot in a table richc keeps, so nothing the caller owns has to
// stay put while it runs. Every thread asks for the same stack, RC_THREAD_STACK_SIZE, on every
// target, so code that fits on one fits on all; some OSes take a guard page and their own
// thread-local storage out of it.
//
// On wasm without threads (a page that is not cross-origin isolated), rc_thread_make always
// fails, and code must carry on without. With threads, threads run on a pool of 16 workers, so at
// most 16 run at once and rc_thread_make fails past that, and each thread has 4 KiB of
// thread-local storage.
//
// The page heap behind every arena is not thread-safe yet, so only one thread at a time may make,
// grow or release arenas.

#ifndef RC_THREAD_THREAD_H_
#define RC_THREAD_THREAD_H_

#include <stdbool.h>
#include <stdint.h>

// A variable with one copy per thread, each starting zeroed or at its initialiser.
#if defined(_MSC_VER) && !defined(__clang__)
#  define RC_THREAD_LOCAL __declspec(thread)
#else
#  define RC_THREAD_LOCAL _Thread_local
#endif

enum {
    RC_THREAD_STACK_SIZE = 256 * 1024,   // bytes of stack for every thread rc_thread_make starts
    RC_THREAD_MAX = 256                  // threads that can be started and not yet joined
};

// A started thread, until it is joined. A zeroed one is not a thread.
typedef struct rc_thread {
    uint32_t index_;   // its slot in the table
    uint32_t gen_;     // the slot's generation, odd while in use
} rc_thread;

// Start a thread running fn(context). The result is not valid if the OS would not start one, or
// RC_THREAD_MAX are already running or waiting to be joined.
rc_thread rc_thread_make(void (*fn)(void *), void *context);

// Whether thread names a started thread that has not been joined.
bool rc_thread_is_valid(rc_thread thread);

// Wait for thread to finish, then free its slot and zero thread. Every started thread must be
// joined once.
void rc_thread_join(rc_thread *thread);

// A number for the calling thread, different from every other running thread's.
uint64_t rc_thread_current_id(void);

// Give the rest of this thread's time slice to another thread, if one is waiting to run.
void rc_thread_yield(void);

// Sleep for at least ns nanoseconds.
void rc_thread_sleep_ns(uint64_t ns);

// How many threads the machine can run at once: its logical cores, and at least 1.
uint32_t rc_thread_hardware_concurrency(void);

#endif // RC_THREAD_THREAD_H_
