// algorithm/hash_index_foreach.template.h
//
// Template for a function that visits every entry of a hash index (hash_index.h), calling a macro
// with each entry's index into your data. The order is the table's, so unspecified, and an index
// with two entries (as in a multimap) comes up twice.
//
//   RC_HASH_INDEX_FOREACH_FUNC  the callback, (index) or with a context (ctx, index) (required)
//   RC_HASH_INDEX_FOREACH_CTX   the type of a context to pass the callback, by pointer
//   RC_HASH_INDEX_FOREACH_NAME  the function's name; defaults to rc_hash_index_foreach
//
// The function is void NAME(table) or NAME(table, ctx), with the table by value, as lookups take
// it. The callback must not change the table: adds and removes move entries about.
//
//   #define RC_HASH_INDEX_FOREACH_CTX mesh
//   #define RC_HASH_INDEX_FOREACH_FUNC(m, i) mesh_weld_vertex(m, i)
//   #define RC_HASH_INDEX_FOREACH_NAME weld_indexed
//   #include "richc/algorithm/hash_index_foreach.template.h"
//   // void weld_indexed(rc_hash_index table, mesh *ctx)

// ---- shared by every instantiation ----

#ifndef RC_ALGORITHM_HASH_INDEX_FOREACH_TEMPLATE_H_
#define RC_ALGORITHM_HASH_INDEX_FOREACH_TEMPLATE_H_

#include "richc/hash_index.h"

#endif // RC_ALGORITHM_HASH_INDEX_FOREACH_TEMPLATE_H_

// ---- per-function names ----

// The fallback keeps IntelliSense happy when it parses this file on its own
#ifndef RC_HASH_INDEX_FOREACH_FUNC
#  define RC_HASH_INDEX_FOREACH_FUNC(index) ((void)(index))
#  error "RC_HASH_INDEX_FOREACH_FUNC must be defined before including richc/algorithm/hash_index_foreach.template.h"
#endif

#ifndef RC_HASH_INDEX_FOREACH_NAME
#  define RC_HASH_INDEX_FOREACH_NAME rc_hash_index_foreach
#endif

// The callback as called, and the parameters that carry the context
#ifdef RC_HASH_INDEX_FOREACH_CTX
#  define RC_HASH_INDEX_FOREACH_CALL_(index) RC_HASH_INDEX_FOREACH_FUNC(ctx, index)
#  define RC_HASH_INDEX_FOREACH_PARAMS_ rc_hash_index table, RC_HASH_INDEX_FOREACH_CTX *ctx
#else
#  define RC_HASH_INDEX_FOREACH_CALL_(index) RC_HASH_INDEX_FOREACH_FUNC(index)
#  define RC_HASH_INDEX_FOREACH_PARAMS_ rc_hash_index table
#endif

// Call the callback with the index of each entry of table, in slot order. An empty slot's hash is
// 0, and every entry's is not.
static inline void RC_HASH_INDEX_FOREACH_NAME(RC_HASH_INDEX_FOREACH_PARAMS_)
{
    for (uint32_t slot = 0; slot < table.cap; slot++) {
        rc_hash_index_slot_ entry = table.slots_[slot];

        if (entry.hash != 0) {
            RC_HASH_INDEX_FOREACH_CALL_(entry.index);
        }
    }
}

// ---- cleanup ----

#undef RC_HASH_INDEX_FOREACH_CALL_
#undef RC_HASH_INDEX_FOREACH_PARAMS_

#undef RC_HASH_INDEX_FOREACH_NAME
#undef RC_HASH_INDEX_FOREACH_CTX
#undef RC_HASH_INDEX_FOREACH_FUNC
