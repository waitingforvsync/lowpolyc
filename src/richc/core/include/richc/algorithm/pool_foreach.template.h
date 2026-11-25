// algorithm/pool_foreach.template.h
//
// Template for a function that visits every live element of a pool, made with pool.template.h,
// in index order, calling a macro with the pool and each element's index. A pool cannot tell a
// free slot from a live one by looking at it, so the function first asks the pool for a bitset of
// its free slots, in a scratch arena.
//
//   RC_POOL_FOREACH_POOL   the pool's full type name, rc_pool_<name> (required)
//   RC_POOL_FOREACH_FUNC   the callback, (pool, index) or with a context (ctx, pool, index)
//                          (required)
//   RC_POOL_FOREACH_CTX    the type of a context to pass the callback, by pointer
//   RC_POOL_FOREACH_CONST  define for a read-only walk, with a const pool
//   RC_POOL_FOREACH_NAME   the function's name; defaults to <pool>_foreach
//
// The function is void NAME(pool, scratch) or NAME(pool, ctx, scratch), with the scratch arena
// taken by value, so the bitset is gone when it returns. The callback may remove the element it
// is given, and may add elements, which are not visited, but must not remove any other.
//
//   #define RC_POOL_FOREACH_POOL rc_pool_face
//   #define RC_POOL_FOREACH_CTX mesh
//   #define RC_POOL_FOREACH_FUNC(m, p, i) mesh_draw_face(m, rc_pool_face_get(p, i))
//   #define RC_POOL_FOREACH_CONST
//   #include "richc/algorithm/pool_foreach.template.h"
//   // void rc_pool_face_foreach(const rc_pool_face *pool, mesh *ctx, rc_arena scratch)

// The fallbacks keep IntelliSense happy when it parses this file on its own
#ifndef RC_POOL_FOREACH_POOL
#  define RC_POOL_FOREACH_POOL rc_pool_int
#  error "RC_POOL_FOREACH_POOL must be defined before including richc/algorithm/pool_foreach.template.h"
#endif

#ifndef RC_POOL_FOREACH_FUNC
#  define RC_POOL_FOREACH_FUNC(pool, index) ((void)(pool), (void)(index))
#  error "RC_POOL_FOREACH_FUNC must be defined before including richc/algorithm/pool_foreach.template.h"
#endif

#ifndef RC_POOL_FOREACH_NAME
#  define RC_POOL_FOREACH_NAME RC_CONCAT(RC_POOL_FOREACH_POOL, _foreach)
#endif

#define RC_POOL_FOREACH_FREE_BITSET_ RC_CONCAT(RC_POOL_FOREACH_POOL, _free_bitset)

// The pool's const-ness
#ifdef RC_POOL_FOREACH_CONST
#  define RC_POOL_FOREACH_QUAL_ const
#else
#  define RC_POOL_FOREACH_QUAL_
#endif

// The callback as called, and the parameters that carry the context
#ifdef RC_POOL_FOREACH_CTX
#  define RC_POOL_FOREACH_CALL_(index) RC_POOL_FOREACH_FUNC(ctx, pool, index)
#  define RC_POOL_FOREACH_PARAMS_ RC_POOL_FOREACH_QUAL_ RC_POOL_FOREACH_POOL *pool, RC_POOL_FOREACH_CTX *ctx
#else
#  define RC_POOL_FOREACH_CALL_(index) RC_POOL_FOREACH_FUNC(pool, index)
#  define RC_POOL_FOREACH_PARAMS_ RC_POOL_FOREACH_QUAL_ RC_POOL_FOREACH_POOL *pool
#endif

// Visit every live element of pool, lowest index first. The free slots are found before the walk,
// and the walk stops at the slots there were then, so elements the callback adds are not visited.
static inline void RC_POOL_FOREACH_NAME(RC_POOL_FOREACH_PARAMS_, rc_arena scratch)
{
    rc_bitset free_slots = RC_POOL_FOREACH_FREE_BITSET_(pool, &scratch);

    for (uint32_t i = 0; i < free_slots.num; i++) {
        if (!rc_bitset_is_set(&free_slots, i)) {
            RC_POOL_FOREACH_CALL_(i);
        }
    }
}

// ---- cleanup ----

#undef RC_POOL_FOREACH_CALL_
#undef RC_POOL_FOREACH_PARAMS_
#undef RC_POOL_FOREACH_QUAL_
#undef RC_POOL_FOREACH_FREE_BITSET_

#undef RC_POOL_FOREACH_NAME
#undef RC_POOL_FOREACH_CONST
#undef RC_POOL_FOREACH_CTX
#undef RC_POOL_FOREACH_FUNC
#undef RC_POOL_FOREACH_POOL
