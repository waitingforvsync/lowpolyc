// algorithms/bitset_foreach.template.h
//
// Template for a function that visits the set bits of a bitset in ascending order, calling a
// macro with each one's index. It is the loop in bitset.h, with first_set and next_set, named.
//
//   RC_BITSET_FOREACH_FUNC  the callback, (index) or with a context (ctx, index) (required)
//   RC_BITSET_FOREACH_CTX   the type of a context to pass the callback, by pointer
//   RC_BITSET_FOREACH_NAME  the function's name; defaults to rc_bitset_foreach
//
// The function is void NAME(bs) or NAME(bs, ctx), with bs a const rc_bitset *. The callback must
// not change the bitset.
//
//   #define RC_BITSET_FOREACH_CTX mesh
//   #define RC_BITSET_FOREACH_FUNC(m, i) mesh_select_face(m, i)
//   #define RC_BITSET_FOREACH_NAME select_faces
//   #include "richc/algorithms/bitset_foreach.template.h"
//   // void select_faces(const rc_bitset *bs, mesh *ctx)

// ---- shared by every instantiation ----

#ifndef RC_ALGORITHMS_BITSET_FOREACH_TEMPLATE_H_
#define RC_ALGORITHMS_BITSET_FOREACH_TEMPLATE_H_

#include "richc/bitset.h"

#endif // RC_ALGORITHMS_BITSET_FOREACH_TEMPLATE_H_

// ---- per-function names ----

// The fallback keeps IntelliSense happy when it parses this file on its own
#ifndef RC_BITSET_FOREACH_FUNC
#  define RC_BITSET_FOREACH_FUNC(index) ((void)(index))
#  error "RC_BITSET_FOREACH_FUNC must be defined before including richc/algorithms/bitset_foreach.template.h"
#endif

#ifndef RC_BITSET_FOREACH_NAME
#  define RC_BITSET_FOREACH_NAME rc_bitset_foreach
#endif

// The callback as called, and the parameters that carry the context
#ifdef RC_BITSET_FOREACH_CTX
#  define RC_BITSET_FOREACH_CALL_(index) RC_BITSET_FOREACH_FUNC(ctx, index)
#  define RC_BITSET_FOREACH_PARAMS_ const rc_bitset *bs, RC_BITSET_FOREACH_CTX *ctx
#else
#  define RC_BITSET_FOREACH_CALL_(index) RC_BITSET_FOREACH_FUNC(index)
#  define RC_BITSET_FOREACH_PARAMS_ const rc_bitset *bs
#endif

// Call the callback with the index of each set bit of bs, lowest first.
static inline void RC_BITSET_FOREACH_NAME(RC_BITSET_FOREACH_PARAMS_)
{
    for (uint32_t i = rc_bitset_first_set(bs); i != RC_INDEX_NONE; i = rc_bitset_next_set(bs, i + 1)) {
        RC_BITSET_FOREACH_CALL_(i);
    }
}

// ---- cleanup ----

#undef RC_BITSET_FOREACH_CALL_
#undef RC_BITSET_FOREACH_PARAMS_

#undef RC_BITSET_FOREACH_NAME
#undef RC_BITSET_FOREACH_CTX
#undef RC_BITSET_FOREACH_FUNC
