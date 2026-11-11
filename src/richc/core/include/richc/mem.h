// mem.h
//
// Raw memory copy, move, fill and compare. On wasm32 the first three become
// single memory.copy / memory.fill instructions.

#ifndef RC_MEM_H_
#define RC_MEM_H_

#include <stdint.h>

#if defined(_MSC_VER) && !defined(__clang__)
#  include <string.h>
#  define RC_MEMCPY_  memcpy
#  define RC_MEMMOVE_ memmove
#  define RC_MEMSET_  memset
#  define RC_MEMCMP_  memcmp
#else
#  define RC_MEMCPY_  __builtin_memcpy
#  define RC_MEMMOVE_ __builtin_memmove
#  define RC_MEMSET_  __builtin_memset
#  define RC_MEMCMP_  __builtin_memcmp
#endif

// Copy size bytes between non-overlapping blocks.
static inline void rc_memcpy(void *dst, const void *src, uint32_t size)
{
    RC_MEMCPY_(dst, src, size);
}

// Copy size bytes between blocks that may overlap.
static inline void rc_memmove(void *dst, const void *src, uint32_t size)
{
    RC_MEMMOVE_(dst, src, size);
}

// Fill size bytes with value.
static inline void rc_memset(void *dst, uint8_t value, uint32_t size)
{
    RC_MEMSET_(dst, value, size);
}

// Compare size bytes: negative, zero or positive, like memcmp.
static inline int32_t rc_memcmp(const void *a, const void *b, uint32_t size)
{
    return RC_MEMCMP_(a, b, size);
}

#endif // RC_MEM_H_
