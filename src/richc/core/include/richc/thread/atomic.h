// thread/atomic.h
//
// Lock-free atomic integers, bools and pointers, each operation taking an explicit memory order.
// C11's <stdatomic.h> is not reliable under MSVC, so this is built on the compiler's own atomics
// instead: the __atomic builtins on clang and gcc, and the _Interlocked intrinsics on MSVC.
//
// Every type is a struct over an unsigned integer of the same width, so it can only be reached
// through these functions, and a zeroed one holds zero, false or NULL. For type t:
//
//   rc_atomic_t_make(value)                         an atomic holding value, to initialise one
//   rc_atomic_t_load(atomic, order)
//   rc_atomic_t_store(atomic, value, order)
//   rc_atomic_t_exchange(atomic, value, order)      returns the value it held
//   rc_atomic_t_compare_exchange(atomic, expected, desired, order)
//                                                   swaps only if it held expected, returning
//                                                   the value it held either way
//
// and for the integers (u8 to u64, i8 to i64) only, each returning the value it held:
//
//   rc_atomic_t_fetch_add / _fetch_sub / _fetch_and / _fetch_or / _fetch_xor(atomic, value, order)
//
// The types are rc_atomic_u8 to rc_atomic_u64, rc_atomic_i8 to rc_atomic_i64, rc_atomic_bool and
// rc_atomic_ptr (a void *). Every target, wasm32 included, has instructions for all four widths,
// so none falls back to a lock.

#ifndef RC_THREAD_ATOMIC_H_
#define RC_THREAD_ATOMIC_H_

#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>

#include "richc/macros.h"

// ---- memory orders ----

// How an atomic operation orders the memory accesses around it, as in C11. There is no consume,
// which compilers treat as acquire anyway; the numbers match the __atomic builtins' own.
typedef enum rc_memory_order {
    RC_MEMORY_ORDER_RELAXED = 0,
    RC_MEMORY_ORDER_ACQUIRE = 2,
    RC_MEMORY_ORDER_RELEASE = 3,
    RC_MEMORY_ORDER_ACQ_REL = 4,
    RC_MEMORY_ORDER_SEQ_CST = 5
} rc_memory_order;


// Whether a load may use order: it can acquire, but has nothing to release.
static inline bool rc_atomic_is_load_order_(rc_memory_order order)
{
    return order != RC_MEMORY_ORDER_RELEASE && order != RC_MEMORY_ORDER_ACQ_REL;
}


// Whether a store may use order: it can release, but has nothing to acquire.
static inline bool rc_atomic_is_store_order_(rc_memory_order order)
{
    return order != RC_MEMORY_ORDER_ACQUIRE && order != RC_MEMORY_ORDER_ACQ_REL;
}


// The order for a compare-exchange that fails, which is only a load, so it keeps the acquire half.
static inline rc_memory_order rc_atomic_failure_order_(rc_memory_order order)
{
    if (order == RC_MEMORY_ORDER_ACQ_REL) {
        return RC_MEMORY_ORDER_ACQUIRE;
    }

    if (order == RC_MEMORY_ORDER_RELEASE) {
        return RC_MEMORY_ORDER_RELAXED;
    }

    return order;
}

// ---- backends ----

#if defined(UINTPTR_MAX) && UINTPTR_MAX == UINT64_MAX
#  define RC_ATOMIC_PTR_BITS_ 64
#else
#  define RC_ATOMIC_PTR_BITS_ 32
#endif

// The name of MSVC's operation op on one width, shared by the two templates
#define RC_ATOMIC_MSVC_OP_(op, bits) RC_CONCAT(RC_CONCAT(rc_atomic_msvc_, op), RC_CONCAT(RC_CONCAT(_, bits), _))

#if defined(__clang__) || defined(__GNUC__)
#  define RC_ATOMIC_GNU_
RC_STATIC_ASSERT(RC_MEMORY_ORDER_RELAXED == __ATOMIC_RELAXED);
RC_STATIC_ASSERT(RC_MEMORY_ORDER_ACQUIRE == __ATOMIC_ACQUIRE);
RC_STATIC_ASSERT(RC_MEMORY_ORDER_RELEASE == __ATOMIC_RELEASE);
RC_STATIC_ASSERT(RC_MEMORY_ORDER_ACQ_REL == __ATOMIC_ACQ_REL);
RC_STATIC_ASSERT(RC_MEMORY_ORDER_SEQ_CST == __ATOMIC_SEQ_CST);

// 2 is always lock-free, so no width falls back to a call into libatomic
RC_STATIC_ASSERT(__GCC_ATOMIC_CHAR_LOCK_FREE == 2);
RC_STATIC_ASSERT(__GCC_ATOMIC_SHORT_LOCK_FREE == 2);
RC_STATIC_ASSERT(__GCC_ATOMIC_INT_LOCK_FREE == 2);
RC_STATIC_ASSERT(__GCC_ATOMIC_LLONG_LOCK_FREE == 2);
RC_STATIC_ASSERT(__GCC_ATOMIC_POINTER_LOCK_FREE == 2);
#elif defined(_MSC_VER)
#  include <intrin.h>

// _ReadWriteBarrier stops only the compiler reordering, which is all x64 needs for acquire and
// release. It is marked deprecated, in favour of C11 atomics MSVC lacks.
#  define RC_ATOMIC_MSVC_COMPILER_BARRIER_() \
    __pragma(warning(push)) __pragma(warning(disable : 4996)) _ReadWriteBarrier() __pragma(warning(pop))

#  if defined(_M_X64)
#    define RC_ATOMIC_MSVC_BARRIER_() RC_ATOMIC_MSVC_COMPILER_BARRIER_()
#  elif defined(_M_ARM64)
#    define RC_ATOMIC_MSVC_BARRIER_() __dmb(_ARM64_BARRIER_ISH)
#  else
#    error "richc needs x64 or ARM64 on MSVC"
#  endif

// The _Interlocked intrinsics take char, short, long and __int64, and only the 32-bit ones have no
// width in their names.
#  define RC_ATOMIC_MSVC_BITS 8
#  define RC_ATOMIC_MSVC_INTERLOCKED char
#  define RC_ATOMIC_MSVC_SUFFIX 8
#  include "richc/thread/private/atomic_msvc.template.h"

#  define RC_ATOMIC_MSVC_BITS 16
#  define RC_ATOMIC_MSVC_INTERLOCKED short
#  define RC_ATOMIC_MSVC_SUFFIX 16
#  include "richc/thread/private/atomic_msvc.template.h"

#  define RC_ATOMIC_MSVC_BITS 32
#  define RC_ATOMIC_MSVC_INTERLOCKED long
#  define RC_ATOMIC_MSVC_SUFFIX
#  include "richc/thread/private/atomic_msvc.template.h"

#  define RC_ATOMIC_MSVC_BITS 64
#  define RC_ATOMIC_MSVC_INTERLOCKED __int64
#  define RC_ATOMIC_MSVC_SUFFIX 64
#  include "richc/thread/private/atomic_msvc.template.h"
#else
#  error "richc's atomics need clang, gcc or MSVC"
#endif

// ---- types ----

#define RC_ATOMIC_TYPE uint8_t
#define RC_ATOMIC_NAME rc_atomic_u8
#define RC_ATOMIC_BITS 8
#define RC_ATOMIC_INTEGER
#include "richc/thread/private/atomic.template.h"

#define RC_ATOMIC_TYPE int8_t
#define RC_ATOMIC_NAME rc_atomic_i8
#define RC_ATOMIC_BITS 8
#define RC_ATOMIC_INTEGER
#include "richc/thread/private/atomic.template.h"

#define RC_ATOMIC_TYPE uint16_t
#define RC_ATOMIC_NAME rc_atomic_u16
#define RC_ATOMIC_BITS 16
#define RC_ATOMIC_INTEGER
#include "richc/thread/private/atomic.template.h"

#define RC_ATOMIC_TYPE int16_t
#define RC_ATOMIC_NAME rc_atomic_i16
#define RC_ATOMIC_BITS 16
#define RC_ATOMIC_INTEGER
#include "richc/thread/private/atomic.template.h"

#define RC_ATOMIC_TYPE uint32_t
#define RC_ATOMIC_NAME rc_atomic_u32
#define RC_ATOMIC_BITS 32
#define RC_ATOMIC_INTEGER
#include "richc/thread/private/atomic.template.h"

#define RC_ATOMIC_TYPE int32_t
#define RC_ATOMIC_NAME rc_atomic_i32
#define RC_ATOMIC_BITS 32
#define RC_ATOMIC_INTEGER
#include "richc/thread/private/atomic.template.h"

#define RC_ATOMIC_TYPE uint64_t
#define RC_ATOMIC_NAME rc_atomic_u64
#define RC_ATOMIC_BITS 64
#define RC_ATOMIC_INTEGER
#include "richc/thread/private/atomic.template.h"

#define RC_ATOMIC_TYPE int64_t
#define RC_ATOMIC_NAME rc_atomic_i64
#define RC_ATOMIC_BITS 64
#define RC_ATOMIC_INTEGER
#include "richc/thread/private/atomic.template.h"

#define RC_ATOMIC_TYPE bool
#define RC_ATOMIC_NAME rc_atomic_bool
#define RC_ATOMIC_BITS 8
#include "richc/thread/private/atomic.template.h"

#define RC_ATOMIC_TYPE void *
#define RC_ATOMIC_NAME rc_atomic_ptr
#define RC_ATOMIC_BITS RC_ATOMIC_PTR_BITS_
#include "richc/thread/private/atomic.template.h"

// ---- fences ----

// Order the memory accesses either side of this as order says, against other threads.
static inline void rc_atomic_thread_fence(rc_memory_order order)
{
#if defined(RC_ATOMIC_GNU_)
    __atomic_thread_fence((int)order);
#else
#  if defined(_M_X64)
    long guard = 0;

    // x64 needs a real fence only for seq_cst, where a locked instruction is cheaper than mfence
    if (order == RC_MEMORY_ORDER_SEQ_CST) {
        (void)_InterlockedIncrement(&guard);
        return;
    }
#  endif

    if (order != RC_MEMORY_ORDER_RELAXED) {
        RC_ATOMIC_MSVC_BARRIER_();
    }
#endif
}


// Order the memory accesses either side of this against the same thread only, such as a signal
// handler interrupting it, which stops the compiler reordering them but emits nothing.
static inline void rc_atomic_signal_fence(rc_memory_order order)
{
#if defined(RC_ATOMIC_GNU_)
    __atomic_signal_fence((int)order);
#else
    (void)order;
    RC_ATOMIC_MSVC_COMPILER_BARRIER_();
#endif
}

#endif // RC_THREAD_ATOMIC_H_
