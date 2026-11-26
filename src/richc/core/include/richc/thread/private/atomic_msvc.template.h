// thread/private/atomic_msvc.template.h
//
// Template for MSVC's atomic operations on one width of unsigned integer, which
// richc/thread/atomic.h instantiates for each width and atomic.template.h calls.
//
// The _Interlocked intrinsics are full barriers on x64 and ARM64, so they need no order, and only
// plain loads and stores add barriers of their own. A seq_cst store is an exchange, after which a
// plain load is enough to be seq_cst too.
//
//   RC_ATOMIC_MSVC_BITS         the width: 8, 16, 32 or 64 (required)
//   RC_ATOMIC_MSVC_INTERLOCKED  the type the _Interlocked intrinsics take at that width (required)
//   RC_ATOMIC_MSVC_SUFFIX       the width in those intrinsics' names, empty for 32 (required)

// The fallbacks keep IntelliSense happy when it parses this file on its own
#ifndef RC_ATOMIC_MSVC_BITS
#  define RC_ATOMIC_MSVC_BITS 32
#  define RC_ATOMIC_MSVC_INTERLOCKED long
#  define RC_ATOMIC_MSVC_SUFFIX
#  error "RC_ATOMIC_MSVC_BITS must be defined before including richc/thread/private/atomic_msvc.template.h"
#endif

#define RC_ATOMIC_MSVC_RAW_ RC_CONCAT(RC_CONCAT(uint, RC_ATOMIC_MSVC_BITS), _t)
#define RC_ATOMIC_MSVC_ISO_ RC_CONCAT(__int, RC_ATOMIC_MSVC_BITS)
#define RC_ATOMIC_MSVC_NAME_(op) RC_ATOMIC_MSVC_OP_(op, RC_ATOMIC_MSVC_BITS)
#define RC_ATOMIC_MSVC_INTRINSIC_(name) RC_CONCAT(name, RC_ATOMIC_MSVC_SUFFIX)
#define RC_ATOMIC_MSVC_POINTER_(raw) ((RC_ATOMIC_MSVC_INTERLOCKED volatile *)(raw))


static inline RC_ATOMIC_MSVC_RAW_ RC_ATOMIC_MSVC_NAME_(load)(const RC_ATOMIC_MSVC_RAW_ *raw, rc_memory_order order)
{
    RC_ATOMIC_MSVC_RAW_ value = (RC_ATOMIC_MSVC_RAW_)RC_CONCAT(__iso_volatile_load, RC_ATOMIC_MSVC_BITS)(
        (const volatile RC_ATOMIC_MSVC_ISO_ *)raw);

    if (order != RC_MEMORY_ORDER_RELAXED) {
        RC_ATOMIC_MSVC_BARRIER_();
    }

    return value;
}


static inline RC_ATOMIC_MSVC_RAW_ RC_ATOMIC_MSVC_NAME_(exchange)(RC_ATOMIC_MSVC_RAW_ *raw, RC_ATOMIC_MSVC_RAW_ value)
{
    return (RC_ATOMIC_MSVC_RAW_)RC_ATOMIC_MSVC_INTRINSIC_(_InterlockedExchange)(RC_ATOMIC_MSVC_POINTER_(raw),
        (RC_ATOMIC_MSVC_INTERLOCKED)value);
}


static inline void RC_ATOMIC_MSVC_NAME_(store)(RC_ATOMIC_MSVC_RAW_ *raw, RC_ATOMIC_MSVC_RAW_ value, rc_memory_order order)
{
    if (order == RC_MEMORY_ORDER_SEQ_CST) {
        (void)RC_ATOMIC_MSVC_NAME_(exchange)(raw, value);
        return;
    }

    if (order != RC_MEMORY_ORDER_RELAXED) {
        RC_ATOMIC_MSVC_BARRIER_();
    }

    RC_CONCAT(__iso_volatile_store, RC_ATOMIC_MSVC_BITS)((volatile RC_ATOMIC_MSVC_ISO_ *)raw, (RC_ATOMIC_MSVC_ISO_)value);
}


static inline RC_ATOMIC_MSVC_RAW_ RC_ATOMIC_MSVC_NAME_(compare_exchange)(RC_ATOMIC_MSVC_RAW_ *raw, RC_ATOMIC_MSVC_RAW_ expected,
    RC_ATOMIC_MSVC_RAW_ desired)
{
    return (RC_ATOMIC_MSVC_RAW_)RC_ATOMIC_MSVC_INTRINSIC_(_InterlockedCompareExchange)(RC_ATOMIC_MSVC_POINTER_(raw),
        (RC_ATOMIC_MSVC_INTERLOCKED)desired, (RC_ATOMIC_MSVC_INTERLOCKED)expected);
}


static inline RC_ATOMIC_MSVC_RAW_ RC_ATOMIC_MSVC_NAME_(fetch_add)(RC_ATOMIC_MSVC_RAW_ *raw, RC_ATOMIC_MSVC_RAW_ value)
{
    return (RC_ATOMIC_MSVC_RAW_)RC_ATOMIC_MSVC_INTRINSIC_(_InterlockedExchangeAdd)(RC_ATOMIC_MSVC_POINTER_(raw),
        (RC_ATOMIC_MSVC_INTERLOCKED)value);
}


static inline RC_ATOMIC_MSVC_RAW_ RC_ATOMIC_MSVC_NAME_(fetch_and)(RC_ATOMIC_MSVC_RAW_ *raw, RC_ATOMIC_MSVC_RAW_ value)
{
    return (RC_ATOMIC_MSVC_RAW_)RC_ATOMIC_MSVC_INTRINSIC_(_InterlockedAnd)(RC_ATOMIC_MSVC_POINTER_(raw),
        (RC_ATOMIC_MSVC_INTERLOCKED)value);
}


static inline RC_ATOMIC_MSVC_RAW_ RC_ATOMIC_MSVC_NAME_(fetch_or)(RC_ATOMIC_MSVC_RAW_ *raw, RC_ATOMIC_MSVC_RAW_ value)
{
    return (RC_ATOMIC_MSVC_RAW_)RC_ATOMIC_MSVC_INTRINSIC_(_InterlockedOr)(RC_ATOMIC_MSVC_POINTER_(raw),
        (RC_ATOMIC_MSVC_INTERLOCKED)value);
}


static inline RC_ATOMIC_MSVC_RAW_ RC_ATOMIC_MSVC_NAME_(fetch_xor)(RC_ATOMIC_MSVC_RAW_ *raw, RC_ATOMIC_MSVC_RAW_ value)
{
    return (RC_ATOMIC_MSVC_RAW_)RC_ATOMIC_MSVC_INTRINSIC_(_InterlockedXor)(RC_ATOMIC_MSVC_POINTER_(raw),
        (RC_ATOMIC_MSVC_INTERLOCKED)value);
}

#undef RC_ATOMIC_MSVC_BITS
#undef RC_ATOMIC_MSVC_INTERLOCKED
#undef RC_ATOMIC_MSVC_SUFFIX
#undef RC_ATOMIC_MSVC_RAW_
#undef RC_ATOMIC_MSVC_ISO_
#undef RC_ATOMIC_MSVC_NAME_
#undef RC_ATOMIC_MSVC_INTRINSIC_
#undef RC_ATOMIC_MSVC_POINTER_
