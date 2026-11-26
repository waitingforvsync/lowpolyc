// thread/private/atomic.template.h
//
// Template for one atomic type, instantiated for each by richc/thread/atomic.h. The value is held
// as an unsigned integer of the same width, so the types of one width share MSVC's operations,
// and no value is ever read through a pointer to another type.
//
//   RC_ATOMIC_TYPE     the value type (required)
//   RC_ATOMIC_NAME     the atomic type's whole name (required), as a suffix of bool would expand
//                      to _Bool when pasted on
//   RC_ATOMIC_BITS     its width: 8, 16, 32 or 64 (required)
//   RC_ATOMIC_INTEGER  also generate the fetch_ arithmetic

// The fallbacks keep IntelliSense happy when it parses this file on its own
#ifndef RC_ATOMIC_TYPE
#  define RC_ATOMIC_TYPE uint32_t
#  define RC_ATOMIC_NAME rc_atomic_u32
#  define RC_ATOMIC_BITS 32
#  error "RC_ATOMIC_TYPE must be defined before including richc/thread/private/atomic.template.h"
#endif

#define RC_ATOMIC_ RC_ATOMIC_NAME
#define RC_ATOMIC_RAW_ RC_CONCAT(RC_CONCAT(uint, RC_ATOMIC_BITS), _t)
#define RC_ATOMIC_MSVC_(op) RC_ATOMIC_MSVC_OP_(op, RC_ATOMIC_BITS)
#define RC_ATOMIC_MAKE_ RC_CONCAT(RC_ATOMIC_, _make)
#define RC_ATOMIC_LOAD_ RC_CONCAT(RC_ATOMIC_, _load)
#define RC_ATOMIC_STORE_ RC_CONCAT(RC_ATOMIC_, _store)
#define RC_ATOMIC_EXCHANGE_ RC_CONCAT(RC_ATOMIC_, _exchange)
#define RC_ATOMIC_COMPARE_EXCHANGE_ RC_CONCAT(RC_ATOMIC_, _compare_exchange)
#define RC_ATOMIC_FETCH_ADD_ RC_CONCAT(RC_ATOMIC_, _fetch_add)
#define RC_ATOMIC_FETCH_SUB_ RC_CONCAT(RC_ATOMIC_, _fetch_sub)
#define RC_ATOMIC_FETCH_AND_ RC_CONCAT(RC_ATOMIC_, _fetch_and)
#define RC_ATOMIC_FETCH_OR_ RC_CONCAT(RC_ATOMIC_, _fetch_or)
#define RC_ATOMIC_FETCH_XOR_ RC_CONCAT(RC_ATOMIC_, _fetch_xor)

typedef struct RC_ATOMIC_ {
    alignas(RC_ATOMIC_BITS / 8) RC_ATOMIC_RAW_ raw_;
} RC_ATOMIC_;


// An atomic holding value, for initialising one; it is not itself atomic.
static inline RC_ATOMIC_ RC_ATOMIC_MAKE_(RC_ATOMIC_TYPE value)
{
    RC_ATOMIC_ atomic = {(RC_ATOMIC_RAW_)value};

    return atomic;
}


// The value atomic holds. order is relaxed, acquire or seq_cst.
static inline RC_ATOMIC_TYPE RC_ATOMIC_LOAD_(const RC_ATOMIC_ *atomic, rc_memory_order order)
{
    RC_ASSERT(rc_atomic_is_load_order_(order));
#if defined(RC_ATOMIC_GNU_)
    return (RC_ATOMIC_TYPE)__atomic_load_n(&atomic->raw_, (int)order);
#else
    return (RC_ATOMIC_TYPE)RC_ATOMIC_MSVC_(load)(&atomic->raw_, order);
#endif
}


// Make atomic hold value. order is relaxed, release or seq_cst.
static inline void RC_ATOMIC_STORE_(RC_ATOMIC_ *atomic, RC_ATOMIC_TYPE value, rc_memory_order order)
{
    RC_ASSERT(rc_atomic_is_store_order_(order));
#if defined(RC_ATOMIC_GNU_)
    __atomic_store_n(&atomic->raw_, (RC_ATOMIC_RAW_)value, (int)order);
#else
    RC_ATOMIC_MSVC_(store)(&atomic->raw_, (RC_ATOMIC_RAW_)value, order);
#endif
}


// Make atomic hold value, returning the value it held.
static inline RC_ATOMIC_TYPE RC_ATOMIC_EXCHANGE_(RC_ATOMIC_ *atomic, RC_ATOMIC_TYPE value, rc_memory_order order)
{
#if defined(RC_ATOMIC_GNU_)
    return (RC_ATOMIC_TYPE)__atomic_exchange_n(&atomic->raw_, (RC_ATOMIC_RAW_)value, (int)order);
#else
    (void)order;
    return (RC_ATOMIC_TYPE)RC_ATOMIC_MSVC_(exchange)(&atomic->raw_, (RC_ATOMIC_RAW_)value);
#endif
}


// Make atomic hold desired if it holds expected, returning the value it held either way, so the
// swap happened if that is expected. A failed swap is a load, ordered as order allows a load.
static inline RC_ATOMIC_TYPE RC_ATOMIC_COMPARE_EXCHANGE_(RC_ATOMIC_ *atomic, RC_ATOMIC_TYPE expected, RC_ATOMIC_TYPE desired,
    rc_memory_order order)
{
#if defined(RC_ATOMIC_GNU_)
    RC_ATOMIC_RAW_ held = (RC_ATOMIC_RAW_)expected;

    (void)__atomic_compare_exchange_n(&atomic->raw_, &held, (RC_ATOMIC_RAW_)desired, false, (int)order,
        (int)rc_atomic_failure_order_(order));
    return (RC_ATOMIC_TYPE)held;
#else
    (void)order;
    return (RC_ATOMIC_TYPE)RC_ATOMIC_MSVC_(compare_exchange)(&atomic->raw_, (RC_ATOMIC_RAW_)expected, (RC_ATOMIC_RAW_)desired);
#endif
}

#if defined(RC_ATOMIC_INTEGER)


// Add value to atomic, wrapping, and return the value it held.
static inline RC_ATOMIC_TYPE RC_ATOMIC_FETCH_ADD_(RC_ATOMIC_ *atomic, RC_ATOMIC_TYPE value, rc_memory_order order)
{
#if defined(RC_ATOMIC_GNU_)
    return (RC_ATOMIC_TYPE)__atomic_fetch_add(&atomic->raw_, (RC_ATOMIC_RAW_)value, (int)order);
#else
    (void)order;
    return (RC_ATOMIC_TYPE)RC_ATOMIC_MSVC_(fetch_add)(&atomic->raw_, (RC_ATOMIC_RAW_)value);
#endif
}


// Subtract value from atomic, wrapping, and return the value it held.
static inline RC_ATOMIC_TYPE RC_ATOMIC_FETCH_SUB_(RC_ATOMIC_ *atomic, RC_ATOMIC_TYPE value, rc_memory_order order)
{
#if defined(RC_ATOMIC_GNU_)
    return (RC_ATOMIC_TYPE)__atomic_fetch_sub(&atomic->raw_, (RC_ATOMIC_RAW_)value, (int)order);
#else
    (void)order;
    return (RC_ATOMIC_TYPE)RC_ATOMIC_MSVC_(fetch_add)(&atomic->raw_, (RC_ATOMIC_RAW_)(0u - (RC_ATOMIC_RAW_)value));
#endif
}


// And value into atomic, returning the value it held.
static inline RC_ATOMIC_TYPE RC_ATOMIC_FETCH_AND_(RC_ATOMIC_ *atomic, RC_ATOMIC_TYPE value, rc_memory_order order)
{
#if defined(RC_ATOMIC_GNU_)
    return (RC_ATOMIC_TYPE)__atomic_fetch_and(&atomic->raw_, (RC_ATOMIC_RAW_)value, (int)order);
#else
    (void)order;
    return (RC_ATOMIC_TYPE)RC_ATOMIC_MSVC_(fetch_and)(&atomic->raw_, (RC_ATOMIC_RAW_)value);
#endif
}


// Or value into atomic, returning the value it held.
static inline RC_ATOMIC_TYPE RC_ATOMIC_FETCH_OR_(RC_ATOMIC_ *atomic, RC_ATOMIC_TYPE value, rc_memory_order order)
{
#if defined(RC_ATOMIC_GNU_)
    return (RC_ATOMIC_TYPE)__atomic_fetch_or(&atomic->raw_, (RC_ATOMIC_RAW_)value, (int)order);
#else
    (void)order;
    return (RC_ATOMIC_TYPE)RC_ATOMIC_MSVC_(fetch_or)(&atomic->raw_, (RC_ATOMIC_RAW_)value);
#endif
}


// Exclusive-or value into atomic, returning the value it held.
static inline RC_ATOMIC_TYPE RC_ATOMIC_FETCH_XOR_(RC_ATOMIC_ *atomic, RC_ATOMIC_TYPE value, rc_memory_order order)
{
#if defined(RC_ATOMIC_GNU_)
    return (RC_ATOMIC_TYPE)__atomic_fetch_xor(&atomic->raw_, (RC_ATOMIC_RAW_)value, (int)order);
#else
    (void)order;
    return (RC_ATOMIC_TYPE)RC_ATOMIC_MSVC_(fetch_xor)(&atomic->raw_, (RC_ATOMIC_RAW_)value);
#endif
}

#endif // RC_ATOMIC_INTEGER

#undef RC_ATOMIC_TYPE
#undef RC_ATOMIC_NAME
#undef RC_ATOMIC_BITS
#undef RC_ATOMIC_INTEGER
#undef RC_ATOMIC_
#undef RC_ATOMIC_RAW_
#undef RC_ATOMIC_MSVC_
#undef RC_ATOMIC_MAKE_
#undef RC_ATOMIC_LOAD_
#undef RC_ATOMIC_STORE_
#undef RC_ATOMIC_EXCHANGE_
#undef RC_ATOMIC_COMPARE_EXCHANGE_
#undef RC_ATOMIC_FETCH_ADD_
#undef RC_ATOMIC_FETCH_SUB_
#undef RC_ATOMIC_FETCH_AND_
#undef RC_ATOMIC_FETCH_OR_
#undef RC_ATOMIC_FETCH_XOR_
