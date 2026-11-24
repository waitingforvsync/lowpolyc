// driver.c
//
// Hashes the bits of all 24 transcendentals' results over many inputs, so that builds for
// different compilers and targets can be shown to compute bit for bit the same. NaNs are
// canonicalised first, since their payloads may differ. Built natively with native.c, and for
// wasm32 with these three functions exported for run.mjs; it uses nothing from libc.
//
//   hash_f1(which, stride)  f32 sin, cos, tan, asin, acos, atan, exp, log, log2, cbrt (which = 0
//                           to 9) on every stride-th bit pattern from which
//   hash_d1(which, count)   their f64 versions on count inputs, alternately ordinary values in
//                           (-1000, 1000) and random bits
//   hash_2(which, count)    f32 atan2 and pow, then f64 atan2 and pow (which = 0 to 3) on count
//                           pairs, alternately ordinary and random bits
//
// Run with run.sh.

#include "richc/math/math.h"

typedef float (*function_f1)(float);
typedef double (*function_d1)(double);

static const function_f1 functions_f1[] = {
    rc_sin_f32, rc_cos_f32, rc_tan_f32, rc_asin_f32, rc_acos_f32, rc_atan_f32, rc_exp_f32, rc_log_f32, rc_log2_f32, rc_cbrt_f32
};

static const function_d1 functions_d1[] = {
    rc_sin_f64, rc_cos_f64, rc_tan_f64, rc_asin_f64, rc_acos_f64, rc_atan_f64, rc_exp_f64, rc_log_f64, rc_log2_f64, rc_cbrt_f64
};

uint64_t hash_f1(uint32_t which, uint32_t stride);
uint64_t hash_d1(uint32_t which, uint32_t count);
uint64_t hash_2(uint32_t which, uint32_t count);


// One step of an FNV-1a style hash, with a shift to spread the high bits back down.
static uint64_t mix(uint64_t h, uint64_t v)
{
    h ^= v;
    h *= 0x100000001b3ull;
    return h ^ (h >> 29);
}


// splitmix64.
static uint64_t next_random(uint64_t *state)
{
    uint64_t z = (*state += 0x9e3779b97f4a7c15ull);

    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}


static uint64_t bits_f32(float x)
{
    return x != x ? 0x7fc00000u : rc_bitcast_f32(x);
}


static uint64_t bits_f64(double x)
{
    return x != x ? 0x7ff8000000000000ull : rc_bitcast_f64(x);
}


uint64_t hash_f1(uint32_t which, uint32_t stride)
{
    uint64_t h = 0xcbf29ce484222325ull;

    for (uint64_t i = which; i < (1ull << 32); i += stride) {
        h = mix(h, bits_f32(functions_f1[which](rc_bitcast_u32((uint32_t)i))));
    }

    return h;
}


uint64_t hash_d1(uint32_t which, uint32_t count)
{
    uint64_t h = 0xcbf29ce484222325ull;
    uint64_t state = which;

    for (uint32_t i = 0; i < count; i++) {
        uint64_t r = next_random(&state);
        double x = (i & 1) ? rc_bitcast_u64(r) : ((double)(r >> 11) * 0x1p-53 - 0.5) * 2000.0;

        h = mix(h, bits_f64(functions_d1[which](x)));
    }

    return h;
}


// The bits of the which-th function of two arguments at (x, y).
static uint64_t bits_2(uint32_t which, double x, double y)
{
    if (which == 0) {
        return bits_f32(rc_atan2_f32((float)y, (float)x));
    }

    if (which == 1) {
        return bits_f32(rc_pow_f32((float)x, (float)y));
    }

    return which == 2 ? bits_f64(rc_atan2_f64(y, x)) : bits_f64(rc_pow_f64(x, y));
}


uint64_t hash_2(uint32_t which, uint32_t count)
{
    uint64_t h = 0xcbf29ce484222325ull;
    uint64_t state = 99 + which;

    for (uint32_t i = 0; i < count; i++) {
        uint64_t r = next_random(&state);
        uint64_t t = next_random(&state);
        bool raw = i & 1;
        double x = raw ? rc_bitcast_u64(r) : (double)(r >> 11) * 0x1p-53 * 100.0;
        double y = raw ? rc_bitcast_u64(t) : ((double)(t >> 11) * 0x1p-53 - 0.5) * 200.0;

        h = mix(h, bits_2(which, x, y));
    }

    return h;
}
