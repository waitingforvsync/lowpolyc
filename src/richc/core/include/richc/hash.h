// hash.h
//
// 32-bit hashes of richc's types, for hash tables.
//
//   integers        the Murmur3 finaliser (32-bit), or splitmix64's finaliser folded to 32 bits
//   floats          their bits, with -0.0 and +0.0 hashing alike, as they compare equal
//   byte sequences  Murmur3 (32-bit)
//   combining       rc_hash_combine mixes another hash into a running one (a splitmix64 step)
//
// A struct hashes field by field:
//
//   uint32_t h = rc_hash_i32(point.x);
//   h = rc_hash_combine(h, rc_hash_i32(point.y));

#ifndef RC_HASH_H_
#define RC_HASH_H_

#include "richc/str.h"

// ---- numbers ----

uint32_t rc_hash_u32(uint32_t x);
uint32_t rc_hash_i32(int32_t x);
uint32_t rc_hash_u64(uint64_t x);
uint32_t rc_hash_i64(int64_t x);
uint32_t rc_hash_f32(float x);
uint32_t rc_hash_f64(double x);

// The pointer's address.
uint32_t rc_hash_ptr(const void *p);

// ---- byte sequences ----

uint32_t rc_hash_bytes(const void *data, uint32_t len);
uint32_t rc_hash_str(rc_str s);

// ---- combining ----

// seed with hash mixed in. Order matters: combining a then b differs from b then a.
uint32_t rc_hash_combine(uint32_t seed, uint32_t hash);

#endif // RC_HASH_H_
