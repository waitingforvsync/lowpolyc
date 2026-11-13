// hash_trie_foreach.template.h
//
// Template for a function that visits every entry of a hash trie, made with hash_trie.template.h.
// It walks the trie's blocks from the root, calling a macro with the pool and each entry's node
// index, through which the callback reads the key and value.
//
//   RC_HASH_TRIE_FOREACH_TRIE   the trie's full type name, rc_hash_trie_<name> (required)
//   RC_HASH_TRIE_FOREACH_FUNC   the callback, (pool, index) or with a context (ctx, pool, index)
//                               (required)
//   RC_HASH_TRIE_FOREACH_CTX    the type of a context to pass the callback, by pointer
//   RC_HASH_TRIE_FOREACH_CONST  define for a read-only walk, with a const pool
//   RC_HASH_TRIE_FOREACH_NAME   the function's name; defaults to <trie>_foreach
//
// The function is void NAME(trie, pool) or NAME(trie, pool, ctx). The order is unspecified. The
// callback may change values (unless the walk is const), but must not add or remove keys.
//
// The walk keeps the blocks still to visit in a local array, not on the call stack, and the trie
// bounds how many there can be (its _walk_max_), so a long chain of colliding keys is safe.

// The fallbacks keep IntelliSense happy when it parses this file on its own
#ifndef RC_HASH_TRIE_FOREACH_TRIE
#  define RC_HASH_TRIE_FOREACH_TRIE rc_hash_trie_int
#  error "RC_HASH_TRIE_FOREACH_TRIE must be defined before including richc/hash_trie_foreach.template.h"
#endif

#ifndef RC_HASH_TRIE_FOREACH_FUNC
#  define RC_HASH_TRIE_FOREACH_FUNC(pool, index) ((void)(pool), (void)(index))
#  error "RC_HASH_TRIE_FOREACH_FUNC must be defined before including richc/hash_trie_foreach.template.h"
#endif

#ifndef RC_HASH_TRIE_FOREACH_NAME
#  define RC_HASH_TRIE_FOREACH_NAME RC_CONCAT(RC_HASH_TRIE_FOREACH_TRIE, _foreach)
#endif

#define RC_HASH_TRIE_FOREACH_POOL_ RC_CONCAT(RC_HASH_TRIE_FOREACH_TRIE, _pool)
#define RC_HASH_TRIE_FOREACH_NODE_ RC_CONCAT(RC_HASH_TRIE_FOREACH_TRIE, _node_)
#define RC_HASH_TRIE_FOREACH_AT_ RC_CONCAT(RC_HASH_TRIE_FOREACH_TRIE, _block_at_const_)
#define RC_HASH_TRIE_FOREACH_WIDTH_ RC_CONCAT(RC_HASH_TRIE_FOREACH_TRIE, _width_)
#define RC_HASH_TRIE_FOREACH_WALK_MAX_ RC_CONCAT(RC_HASH_TRIE_FOREACH_TRIE, _walk_max_)

// The pool's const-ness
#ifdef RC_HASH_TRIE_FOREACH_CONST
#  define RC_HASH_TRIE_FOREACH_QUAL_ const
#else
#  define RC_HASH_TRIE_FOREACH_QUAL_
#endif

// The callback as called, and the parameters that carry the context
#ifdef RC_HASH_TRIE_FOREACH_CTX
#  define RC_HASH_TRIE_FOREACH_CALL_(index) RC_HASH_TRIE_FOREACH_FUNC(ctx, pool, index)
#  define RC_HASH_TRIE_FOREACH_PARAMS_ RC_HASH_TRIE_FOREACH_QUAL_ RC_HASH_TRIE_FOREACH_POOL_ *pool, RC_HASH_TRIE_FOREACH_CTX *ctx
#else
#  define RC_HASH_TRIE_FOREACH_CALL_(index) RC_HASH_TRIE_FOREACH_FUNC(pool, index)
#  define RC_HASH_TRIE_FOREACH_PARAMS_ RC_HASH_TRIE_FOREACH_QUAL_ RC_HASH_TRIE_FOREACH_POOL_ *pool
#endif

// Visit every entry of trie. An empty trie visits nothing. Each block's keys are visited, and its
// child blocks put on the waiting list, before the next block is taken from it. A node's link is
// read before its callback, so the callback's changes to values cannot upset the walk.
static inline void RC_HASH_TRIE_FOREACH_NAME(RC_HASH_TRIE_FOREACH_TRIE trie, RC_HASH_TRIE_FOREACH_PARAMS_)
{
    if (trie.root_ == 0) {
        return;
    }

    uint32_t waiting[RC_HASH_TRIE_FOREACH_WALK_MAX_];
    uint32_t num_waiting = 1;
    waiting[0] = trie.root_ - 1;
    while (num_waiting > 0) {
        num_waiting--;

        uint32_t block = waiting[num_waiting];
        for (uint32_t slot = 0; slot < RC_HASH_TRIE_FOREACH_WIDTH_; slot++) {
            const RC_HASH_TRIE_FOREACH_NODE_ *node = &RC_HASH_TRIE_FOREACH_AT_(pool, block)->nodes[slot];
            uint32_t child = node->child_;

            if (child == RC_HASH_TRIE_EMPTY_) {
                continue;
            }
            if (child >= RC_HASH_TRIE_BLOCK_LINK_) {
                RC_ASSERT(num_waiting < RC_HASH_TRIE_FOREACH_WALK_MAX_);
                waiting[num_waiting] = child - RC_HASH_TRIE_BLOCK_LINK_;
                num_waiting++;
            }
            RC_HASH_TRIE_FOREACH_CALL_(block * RC_HASH_TRIE_FOREACH_WIDTH_ + slot);
        }
    }
}

// ---- cleanup ----

#undef RC_HASH_TRIE_FOREACH_CALL_
#undef RC_HASH_TRIE_FOREACH_PARAMS_
#undef RC_HASH_TRIE_FOREACH_QUAL_
#undef RC_HASH_TRIE_FOREACH_WALK_MAX_
#undef RC_HASH_TRIE_FOREACH_WIDTH_
#undef RC_HASH_TRIE_FOREACH_AT_
#undef RC_HASH_TRIE_FOREACH_NODE_
#undef RC_HASH_TRIE_FOREACH_POOL_

#undef RC_HASH_TRIE_FOREACH_NAME
#undef RC_HASH_TRIE_FOREACH_CONST
#undef RC_HASH_TRIE_FOREACH_CTX
#undef RC_HASH_TRIE_FOREACH_FUNC
#undef RC_HASH_TRIE_FOREACH_TRIE
