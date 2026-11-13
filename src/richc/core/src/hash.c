// hash.c
//
// The hashes.

#include "richc/hash.h"

#include "richc/ops.h"

// ---- numbers ----

uint32_t rc_hash_u32(uint32_t x)
{
    // Murmur3's finaliser
    x ^= x >> 16;
    x *= 0x85EBCA6Bu;
    x ^= x >> 13;
    x *= 0xC2B2AE35u;
    x ^= x >> 16;
    return x;
}


uint32_t rc_hash_i32(int32_t x)
{
    return rc_hash_u32((uint32_t)x);
}


uint32_t rc_hash_u64(uint64_t x)
{
    // splitmix64's finaliser, folded to 32 bits
    x ^= x >> 33;
    x *= UINT64_C(0xFF51AFD7ED558CCD);
    x ^= x >> 33;
    x *= UINT64_C(0xC4CEB9FE1A85EC53);
    x ^= x >> 33;
    return (uint32_t)x ^ (uint32_t)(x >> 32);
}


uint32_t rc_hash_i64(int64_t x)
{
    return rc_hash_u64((uint64_t)x);
}


uint32_t rc_hash_f32(float x)
{
    // -0.0 and +0.0 compare equal, so must hash alike
    return rc_hash_u32(x == 0.0f ? 0u : rc_bitcast_f32(x));
}


uint32_t rc_hash_f64(double x)
{
    return rc_hash_u64(x == 0.0 ? 0u : rc_bitcast_f64(x));
}


uint32_t rc_hash_ptr(const void *p)
{
    return rc_hash_u64((uint64_t)(uintptr_t)p);
}


// ---- byte sequences ----

// x rotated left by n bits, 0 < n < 32.
static uint32_t rc_hash_rotl(uint32_t x, uint32_t n)
{
    return (x << n) | (x >> (32 - n));
}


// One Murmur3 block: 4 bytes as a little-endian word, scrambled before they are mixed in.
static uint32_t rc_hash_scramble(uint32_t k)
{
    k *= 0xCC9E2D51u;
    k = rc_hash_rotl(k, 15);
    k *= 0x1B873593u;
    return k;
}


uint32_t rc_hash_bytes(const void *data, uint32_t len)
{
    // Murmur3, 32-bit, seed 0: the 4-byte blocks, then the last 1 to 3 bytes, then the length,
    // and finally the finaliser. The blocks are read a byte at a time, so data needs no alignment.
    const uint8_t *bytes = data;
    uint32_t num_blocks = len / 4;
    uint32_t h = 0;

    for (uint32_t i = 0; i < num_blocks; i++) {
        const uint8_t *b = bytes + i * 4;
        uint32_t k = (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;

        h ^= rc_hash_scramble(k);
        h = rc_hash_rotl(h, 13);
        h = h * 5 + 0xE6546B64u;
    }

    // A missing tail scrambles to 0, so changes nothing
    uint32_t tail = 0;
    for (uint32_t i = num_blocks * 4; i < len; i++) {
        tail |= (uint32_t)bytes[i] << (8 * (i % 4));
    }
    h ^= rc_hash_scramble(tail);
    h ^= len;
    return rc_hash_u32(h);
}


uint32_t rc_hash_str(rc_str s)
{
    return rc_hash_bytes(s.data, s.len);
}


// ---- combining ----

uint32_t rc_hash_combine(uint32_t seed, uint32_t hash)
{
    // One splitmix64 step: the pair as one 64-bit value, plus the golden ratio, then the 64-bit
    // hash. The seed's half and the hash's half differ, so the order matters, and the constant
    // keeps zeros from combining to zero.
    uint64_t pair = (uint64_t)seed << 32 | hash;

    return rc_hash_u64(pair + UINT64_C(0x9E3779B97F4A7C15));
}
