// ops.h
//
// Scalar bit operations. The counts are the compiler's builtins: each becomes a bit-scan or count
// instruction (bsr, bsf and popcnt on x86-64-v2; i32.clz, i32.ctz and i32.popcnt on wasm).
//
// The functions carry a type suffix, which also keeps them clear of the min and max macros that
// windows.h defines.

#ifndef RC_OPS_H_
#define RC_OPS_H_

#include "richc/macros.h"

// ---- counting bits ----

// The number of zero bits above the highest set bit; 32 for 0.
static inline uint32_t rc_clz_u32(uint32_t a)
{
    if (a == 0) {
        return 32;
    }
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index = 0;

    _BitScanReverse(&index, a);
    return 31u - (uint32_t)index;
#else
    return (uint32_t)__builtin_clz(a);
#endif
}

// The number of zero bits above the highest set bit; 64 for 0.
static inline uint32_t rc_clz_u64(uint64_t a)
{
    if (a == 0) {
        return 64;
    }
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index = 0;

    _BitScanReverse64(&index, a);
    return 63u - (uint32_t)index;
#else
    return (uint32_t)__builtin_clzll(a);
#endif
}

// The number of zero bits below the lowest set bit; 32 for 0.
static inline uint32_t rc_ctz_u32(uint32_t a)
{
    if (a == 0) {
        return 32;
    }
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index = 0;

    _BitScanForward(&index, a);
    return (uint32_t)index;
#else
    return (uint32_t)__builtin_ctz(a);
#endif
}

// The number of zero bits below the lowest set bit; 64 for 0.
static inline uint32_t rc_ctz_u64(uint64_t a)
{
    if (a == 0) {
        return 64;
    }
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index = 0;

    _BitScanForward64(&index, a);
    return (uint32_t)index;
#else
    return (uint32_t)__builtin_ctzll(a);
#endif
}

// The number of set bits.
static inline uint32_t rc_popcount_u32(uint32_t a)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return (uint32_t)__popcnt(a);
#else
    return (uint32_t)__builtin_popcount(a);
#endif
}

// ---- bit patterns ----

// The bits of a float, as a uint32_t.
static inline uint32_t rc_bitcast_f32(float x)
{
    union {
        float f;
        uint32_t u;
    } pun = {.f = x};

    return pun.u;
}

// The bits of a double, as a uint64_t.
static inline uint64_t rc_bitcast_f64(double x)
{
    union {
        double f;
        uint64_t u;
    } pun = {.f = x};

    return pun.u;
}

#endif // RC_OPS_H_
