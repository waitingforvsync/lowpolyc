// os_thread.c
//
// wasm32. With threads, every thread runs on one of a fixed pool of workers, which the host
// (threads.js) starts before the module runs, as a browser starts a worker only once the page's
// main thread has returned to it. Each worker instantiates the module on the same shared memory,
// sets its stack pointer, and calls rc_wasm_thread_worker, which never returns: it sleeps until a
// thread is handed to it, runs it, and waits for the next. Stacks and thread-local storage are
// static, so a worker costs the heap nothing.
//
// Without threads, no thread can start and there is nothing to yield to, and a sleep can only
// pass the time.

#include "platform/os_thread.h"

#include "richc/macros.h"
#include "richc/time.h"

#if defined(__wasm_atomics__)

#include <stdalign.h>
#include <stddef.h>

#include "platform/futex.h"
#include "richc/thread/thread.h"
#include "wasm_thread.h"

enum {
    RC_WASM_THREADS = 16,   // workers in the pool
    RC_WASM_TLS_MAX = 4096  // bytes of thread-local storage each thread may have
};

// A worker's state, which its handle's starter and joiner, and the worker itself, move it on
// through in this order. Only the joiner makes a finished worker idle again.
enum {
    RC_WASM_WORKER_IDLE = 0,
    RC_WASM_WORKER_CLAIMED = 1,    // a starter has it, and is handing it a thread
    RC_WASM_WORKER_ASSIGNED = 2,   // it has a thread to run, or is running it
    RC_WASM_WORKER_FINISHED = 3    // the thread has returned, and waits to be joined
};

// One worker of the pool.
typedef struct rc_wasm_worker {
    rc_atomic_u32 state;
    void *slot;   // what to pass rc_thread_run_, set while claimed
} rc_wasm_worker;

static rc_wasm_worker rc_wasm_workers[RC_WASM_THREADS];
static uint32_t rc_wasm_core_count = 1;
static alignas(16) uint8_t rc_wasm_stacks[RC_WASM_THREADS][RC_THREAD_STACK_SIZE];

// The main thread's thread-local storage, then each worker's.
static alignas(16) uint8_t rc_wasm_tls[RC_WASM_THREADS + 1][RC_WASM_TLS_MAX];

static RC_THREAD_LOCAL bool rc_wasm_may_block;

// Made by wasm-ld: copy the thread-local initialisers to memory and point __tls_base at it.
extern void __wasm_init_tls(void *memory);


// Whether thread-local storage of size and align fits in a thread's block. Inlined, the compare
// against __builtin_wasm_tls_size crashes clang 21 with SIMD and atomics on.
__attribute__((noinline)) static bool rc_wasm_tls_fits(size_t size, size_t align)
{
    return size <= RC_WASM_TLS_MAX && align <= 16;
}


// Give the calling thread its thread-local storage, at memory.
static void rc_wasm_init_tls(uint8_t *memory)
{
    RC_PANIC(rc_wasm_tls_fits(__builtin_wasm_tls_size(), __builtin_wasm_tls_align()));
    __wasm_init_tls(memory);
}


// Called by the host on the main thread before anything else, as its thread-local storage starts
// at address 0, over the stack. cores is how many the machine has, and may_block whether this
// thread may sleep (under node, not on a page). Returns how many workers to start.
__attribute__((export_name("rc_wasm_threads_init")))
uint32_t rc_wasm_threads_init(uint32_t cores, bool may_block)
{
    rc_wasm_init_tls(rc_wasm_tls[0]);
    rc_wasm_may_block = may_block;
    rc_wasm_core_count = cores > 0 ? cores : 1;
    return RC_WASM_THREADS;
}


// Where worker index's stack starts, for the host to set its stack pointer to before calling in.
__attribute__((export_name("rc_wasm_thread_stack_top")))
uintptr_t rc_wasm_thread_stack_top(uint32_t index)
{
    RC_PANIC(index < RC_WASM_THREADS);
    return (uintptr_t)(rc_wasm_stacks[index] + RC_THREAD_STACK_SIZE);
}


// Run worker index for ever, once the host has set its stack pointer.
__attribute__((export_name("rc_wasm_thread_worker")))
void rc_wasm_thread_worker(uint32_t index)
{
    rc_wasm_worker *worker = &rc_wasm_workers[index];

    RC_PANIC(index < RC_WASM_THREADS);
    rc_wasm_init_tls(rc_wasm_tls[index + 1]);
    rc_wasm_may_block = true;

    for (;;) {
        uint32_t state = rc_atomic_u32_load(&worker->state, RC_MEMORY_ORDER_ACQUIRE);

        if (state != RC_WASM_WORKER_ASSIGNED) {
            (void)rc_futex_wait(&worker->state, state, RC_FUTEX_FOREVER);
            continue;
        }

        rc_thread_run_(worker->slot);
        rc_atomic_u32_store(&worker->state, RC_WASM_WORKER_FINISHED, RC_MEMORY_ORDER_RELEASE);
        rc_futex_wake_all(&worker->state);
    }
}


bool rc_wasm_thread_may_block(void)
{
    return rc_wasm_may_block;
}


uintptr_t rc_os_thread_start(void *slot, uint32_t stack_size)
{
    RC_ASSERT(stack_size == RC_THREAD_STACK_SIZE);

    for (uint32_t i = 0; i < RC_WASM_THREADS; i++) {
        rc_wasm_worker *worker = &rc_wasm_workers[i];

        if (rc_atomic_u32_compare_exchange(&worker->state, RC_WASM_WORKER_IDLE, RC_WASM_WORKER_CLAIMED, RC_MEMORY_ORDER_ACQUIRE)
            == RC_WASM_WORKER_IDLE) {
            worker->slot = slot;
            rc_atomic_u32_store(&worker->state, RC_WASM_WORKER_ASSIGNED, RC_MEMORY_ORDER_RELEASE);
            rc_futex_wake_all(&worker->state);
            return i + 1;
        }
    }

    return 0;
}


void rc_os_thread_join(uintptr_t handle)
{
    rc_wasm_worker *worker = &rc_wasm_workers[handle - 1];

    for (;;) {
        uint32_t state = rc_atomic_u32_load(&worker->state, RC_MEMORY_ORDER_ACQUIRE);

        if (state == RC_WASM_WORKER_FINISHED) {
            break;
        }
        (void)rc_futex_wait(&worker->state, state, RC_FUTEX_FOREVER);
    }

    // The worker is waiting for its next thread already, so it needs no wake
    rc_atomic_u32_store(&worker->state, RC_WASM_WORKER_IDLE, RC_MEMORY_ORDER_RELEASE);
}


void rc_os_thread_yield(void)
{
}


void rc_os_thread_sleep_ns(uint64_t ns)
{
    rc_atomic_u32 never = {0};
    uint64_t start = rc_time_now_ns();
    uint64_t elapsed = 0;

    // Nothing changes never, so each wait runs to its timeout, or spins there on the main thread
    while (elapsed < ns) {
        (void)rc_futex_wait(&never, 0, ns - elapsed);
        elapsed = rc_time_now_ns() - start;
    }
}


uint32_t rc_os_thread_core_count(void)
{
    return rc_wasm_core_count < RC_WASM_THREADS ? rc_wasm_core_count : RC_WASM_THREADS;
}

#else


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

#endif
