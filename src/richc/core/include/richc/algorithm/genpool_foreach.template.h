// algorithm/genpool_foreach.template.h
//
// Template for a function that visits every live element of a genpool, made with
// genpool.template.h, in slot order, calling a macro with the pool and each element's handle. A
// slot is live while its generation is odd, so unlike pool_foreach this needs no scratch memory.
//
//   RC_GENPOOL_FOREACH_POOL   the genpool's full type name, rc_genpool_<name> (required)
//   RC_GENPOOL_FOREACH_FUNC   the callback, (pool, handle) or with a context (ctx, pool, handle)
//                             (required)
//   RC_GENPOOL_FOREACH_CTX    the type of a context to pass the callback, by pointer
//   RC_GENPOOL_FOREACH_CONST  define for a read-only walk, with a const pool
//   RC_GENPOOL_FOREACH_NAME   the function's name; defaults to <pool>_foreach
//
// The function is void NAME(pool) or NAME(pool, ctx). The callback may remove the element it is
// given, and may add elements, which may or may not be visited, but must not remove any other.
//
//   #define RC_GENPOOL_FOREACH_POOL rc_genpool_light
//   #define RC_GENPOOL_FOREACH_CTX scene
//   #define RC_GENPOOL_FOREACH_FUNC(s, p, h) scene_add_light(s, rc_genpool_light_get(p, h))
//   #define RC_GENPOOL_FOREACH_CONST
//   #include "richc/algorithm/genpool_foreach.template.h"
//   // void rc_genpool_light_foreach(const rc_genpool_light *pool, scene *ctx)

// The fallbacks keep IntelliSense happy when it parses this file on its own
#ifndef RC_GENPOOL_FOREACH_POOL
#  define RC_GENPOOL_FOREACH_POOL rc_genpool_int
#  error "RC_GENPOOL_FOREACH_POOL must be defined before including richc/algorithm/genpool_foreach.template.h"
#endif

#ifndef RC_GENPOOL_FOREACH_FUNC
#  define RC_GENPOOL_FOREACH_FUNC(pool, handle) ((void)(pool), (void)(handle))
#  error "RC_GENPOOL_FOREACH_FUNC must be defined before including richc/algorithm/genpool_foreach.template.h"
#endif

#ifndef RC_GENPOOL_FOREACH_NAME
#  define RC_GENPOOL_FOREACH_NAME RC_CONCAT(RC_GENPOOL_FOREACH_POOL, _foreach)
#endif

#define RC_GENPOOL_FOREACH_NUM_SLOTS_ RC_CONCAT(RC_GENPOOL_FOREACH_POOL, _num_slots)
#define RC_GENPOOL_FOREACH_IS_LIVE_ RC_CONCAT(RC_GENPOOL_FOREACH_POOL, _is_live)
#define RC_GENPOOL_FOREACH_HANDLE_AT_ RC_CONCAT(RC_GENPOOL_FOREACH_POOL, _handle_at)

// The pool's const-ness
#ifdef RC_GENPOOL_FOREACH_CONST
#  define RC_GENPOOL_FOREACH_QUAL_ const
#else
#  define RC_GENPOOL_FOREACH_QUAL_
#endif

// The callback as called, and the parameters that carry the context
#ifdef RC_GENPOOL_FOREACH_CTX
#  define RC_GENPOOL_FOREACH_CALL_(handle) RC_GENPOOL_FOREACH_FUNC(ctx, pool, handle)
#  define RC_GENPOOL_FOREACH_PARAMS_ RC_GENPOOL_FOREACH_QUAL_ RC_GENPOOL_FOREACH_POOL *pool, RC_GENPOOL_FOREACH_CTX *ctx
#else
#  define RC_GENPOOL_FOREACH_CALL_(handle) RC_GENPOOL_FOREACH_FUNC(pool, handle)
#  define RC_GENPOOL_FOREACH_PARAMS_ RC_GENPOOL_FOREACH_QUAL_ RC_GENPOOL_FOREACH_POOL *pool
#endif

// Visit every live element of pool, lowest slot first. The walk stops at the slots there were when
// it began, so an element the callback adds is visited only if it reuses a later free slot.
static inline void RC_GENPOOL_FOREACH_NAME(RC_GENPOOL_FOREACH_PARAMS_)
{
    uint32_t num_slots = RC_GENPOOL_FOREACH_NUM_SLOTS_(pool);

    for (uint32_t i = 0; i < num_slots; i++) {
        if (RC_GENPOOL_FOREACH_IS_LIVE_(pool, i)) {
            RC_GENPOOL_FOREACH_CALL_(RC_GENPOOL_FOREACH_HANDLE_AT_(pool, i));
        }
    }
}

// ---- cleanup ----

#undef RC_GENPOOL_FOREACH_CALL_
#undef RC_GENPOOL_FOREACH_PARAMS_
#undef RC_GENPOOL_FOREACH_QUAL_
#undef RC_GENPOOL_FOREACH_HANDLE_AT_
#undef RC_GENPOOL_FOREACH_IS_LIVE_
#undef RC_GENPOOL_FOREACH_NUM_SLOTS_

#undef RC_GENPOOL_FOREACH_NAME
#undef RC_GENPOOL_FOREACH_CONST
#undef RC_GENPOOL_FOREACH_CTX
#undef RC_GENPOOL_FOREACH_FUNC
#undef RC_GENPOOL_FOREACH_POOL
