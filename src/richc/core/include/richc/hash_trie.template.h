// hash_trie.template.h
//
// Template for a hash trie: a set or map whose keys sit in a trie, indexed by successive groups of
// bits of each key's 32-bit hash. The nodes are stored a block at a time, in a pool of blocks that
// any number of tries can share.
//
//   RC_HASH_TRIE_KEY_TYPE    the key type (required)
//   RC_HASH_TRIE_VALUE_TYPE  the value type: define it for a map, leave it out for a set
//   RC_HASH_TRIE_HASH(key)   the key's hash, converted to uint32_t (required)
//   RC_HASH_TRIE_EQUAL(a, b) key equality; defaults to (a) == (b)
//   RC_HASH_TRIE_WIDTH       the nodes in a block: 2, 4, 8 or 16; defaults to 4
//   RC_HASH_TRIE_NAME        the name suffix; defaults to RC_HASH_TRIE_KEY_TYPE, which must then
//                            be one identifier
//
// The trie is rc_hash_trie_<name> and its pool rc_hash_trie_<name>_pool; {0} is an empty one of
// each. The trie is only its root block's index, so it is copied freely, and copies share their
// entries. The pool is passed to every operation. Reads and remove take the trie by value; add,
// set and release take it by pointer, since the first add makes its root block and release
// clears it.
//
// Entries are named by node index: the block index times the width, plus the slot. add, set and
// find return one, and key_get and the value_ functions take one. An index survives adds, but a
// remove can move another key's entry to a new index.
//
// add leaves an existing key's value alone, and set replaces it. A set has no values, so no set
// or value_ functions.
//
// Wider blocks make shallower tries, but leave more slots empty: at width 4 a trie uses about 2
// slots per key, and at width 16 about 5.
//
// How it works
//
// A node is a key, its value, its hash, and child_: RC_HASH_TRIE_EMPTY_, RC_HASH_TRIE_LEAF_ for a
// key with no children, or else the index of its child block plus RC_HASH_TRIE_BLOCK_LINK_. The
// trie's root_ is its root block's index plus one, so 0 means no root yet.
//
// At each level the low bits of the hash pick a slot (2 bits at width 4), and the hash rotates
// right by as many for the next level. A key that finds its slot taken by another key moves a
// level down, so every node with children also holds a key. Lookups compare the stored hashes
// before the keys, so a mismatch rarely compares keys.
//
// Once a path has used all 32 bits (16 levels at width 4), every key below it has the same hash,
// so takes the same slot at every level: such keys form a chain a level per key, slow but
// correct. So only a block in the first levels can hold more than one key, and a walk over the
// trie has a bounded number of blocks waiting, however long a chain gets.
//
// Removing a key with children moves the first key of its child block up into its place, then
// does the same for that key's old node, down to a key with no children, whose slot empties. Only
// that last block can empty, and if it is not the root, it is unlinked and goes back to the pool.

// ---- shared by every instantiation ----

#ifndef RC_HASH_TRIE_TEMPLATE_H_
#define RC_HASH_TRIE_TEMPLATE_H_

#include "richc/arena.h"

// A node's child_ is RC_HASH_TRIE_EMPTY_, RC_HASH_TRIE_LEAF_ for a key with no children, or its
// child block's index plus RC_HASH_TRIE_BLOCK_LINK_
#define RC_HASH_TRIE_EMPTY_ 0u
#define RC_HASH_TRIE_LEAF_ 1u
#define RC_HASH_TRIE_BLOCK_LINK_ 2u

// The hash rotated right by bits, bringing the next level's bits to the bottom.
static inline uint32_t rc_hash_trie_rotate_(uint32_t hash, uint32_t bits)
{
    return (hash >> bits) | (hash << (32 - bits));
}

#endif // RC_HASH_TRIE_TEMPLATE_H_

// ---- per-type names ----

// The fallbacks keep IntelliSense happy when it parses this file on its own
#ifndef RC_HASH_TRIE_KEY_TYPE
#  define RC_HASH_TRIE_KEY_TYPE int
#  error "RC_HASH_TRIE_KEY_TYPE must be defined before including richc/hash_trie.template.h"
#endif

#ifndef RC_HASH_TRIE_HASH
#  define RC_HASH_TRIE_HASH(key) (key)
#  error "RC_HASH_TRIE_HASH must be defined before including richc/hash_trie.template.h"
#endif

#ifndef RC_HASH_TRIE_EQUAL
#  define RC_HASH_TRIE_EQUAL(a, b) ((a) == (b))
#endif

#ifndef RC_HASH_TRIE_WIDTH
#  define RC_HASH_TRIE_WIDTH 4
#endif

#ifndef RC_HASH_TRIE_NAME
#  define RC_HASH_TRIE_NAME RC_HASH_TRIE_KEY_TYPE
#endif

// The hash bits each level uses, and the levels it takes to use all 32
#if RC_HASH_TRIE_WIDTH == 2
#  define RC_HASH_TRIE_BITS_ 1u
#elif RC_HASH_TRIE_WIDTH == 4
#  define RC_HASH_TRIE_BITS_ 2u
#elif RC_HASH_TRIE_WIDTH == 8
#  define RC_HASH_TRIE_BITS_ 3u
#elif RC_HASH_TRIE_WIDTH == 16
#  define RC_HASH_TRIE_BITS_ 4u
#else
#  error "RC_HASH_TRIE_WIDTH must be 2, 4, 8 or 16"
#endif
#define RC_HASH_TRIE_LEVELS_ ((32u + RC_HASH_TRIE_BITS_ - 1u) / RC_HASH_TRIE_BITS_)
#define RC_HASH_TRIE_SLOT_MASK_ ((uint32_t)RC_HASH_TRIE_WIDTH - 1u)

#define RC_HASH_TRIE_ RC_CONCAT(rc_hash_trie_, RC_HASH_TRIE_NAME)
#define RC_HASH_TRIE_POOL_ RC_CONCAT(RC_HASH_TRIE_, _pool)
#define RC_HASH_TRIE_NODE_ RC_CONCAT(RC_HASH_TRIE_, _node_)
#define RC_HASH_TRIE_BLOCK_ RC_CONCAT(RC_HASH_TRIE_, _block_)
#define RC_HASH_TRIE_BLOCKS_ RC_CONCAT(rc_pool_hash_trie_, RC_CONCAT(RC_HASH_TRIE_NAME, _block_))
#define RC_HASH_TRIE_WIDTH_CONST_ RC_CONCAT(RC_HASH_TRIE_, _width_)
#define RC_HASH_TRIE_WALK_MAX_ RC_CONCAT(RC_HASH_TRIE_, _walk_max_)

#define RC_HASH_TRIE_POOL_MAKE_ RC_CONCAT(RC_HASH_TRIE_, _pool_make)
#define RC_HASH_TRIE_POOL_RESERVE_ RC_CONCAT(RC_HASH_TRIE_, _pool_reserve)
#define RC_HASH_TRIE_POOL_RESET_ RC_CONCAT(RC_HASH_TRIE_, _pool_reset)
#define RC_HASH_TRIE_POOL_RELEASE_ RC_CONCAT(RC_HASH_TRIE_, _pool_release)
#define RC_HASH_TRIE_RELEASE_ RC_CONCAT(RC_HASH_TRIE_, _release)
#define RC_HASH_TRIE_CONTAINS_ RC_CONCAT(RC_HASH_TRIE_, _contains)
#define RC_HASH_TRIE_FIND_ RC_CONCAT(RC_HASH_TRIE_, _find)
#define RC_HASH_TRIE_ADD_ RC_CONCAT(RC_HASH_TRIE_, _add)
#define RC_HASH_TRIE_SET_ RC_CONCAT(RC_HASH_TRIE_, _set)
#define RC_HASH_TRIE_REMOVE_ RC_CONCAT(RC_HASH_TRIE_, _remove)
#define RC_HASH_TRIE_KEY_GET_ RC_CONCAT(RC_HASH_TRIE_, _key_get)
#define RC_HASH_TRIE_VALUE_GET_ RC_CONCAT(RC_HASH_TRIE_, _value_get)
#define RC_HASH_TRIE_VALUE_SET_ RC_CONCAT(RC_HASH_TRIE_, _value_set)
#define RC_HASH_TRIE_VALUE_AT_ RC_CONCAT(RC_HASH_TRIE_, _value_at)

#define RC_HASH_TRIE_ADD_BLOCK_ RC_CONCAT(RC_HASH_TRIE_, _add_block_)
#define RC_HASH_TRIE_BLOCK_AT_ RC_CONCAT(RC_HASH_TRIE_, _block_at_)
#define RC_HASH_TRIE_BLOCK_AT_CONST_ RC_CONCAT(RC_HASH_TRIE_, _block_at_const_)
#define RC_HASH_TRIE_NODE_AT_ RC_CONCAT(RC_HASH_TRIE_, _node_at_)
#define RC_HASH_TRIE_NODE_AT_CONST_ RC_CONCAT(RC_HASH_TRIE_, _node_at_const_)
#define RC_HASH_TRIE_BLOCK_IS_EMPTY_ RC_CONCAT(RC_HASH_TRIE_, _block_is_empty_)
#define RC_HASH_TRIE_PROBE_ RC_CONCAT(RC_HASH_TRIE_, _probe_)
#define RC_HASH_TRIE_INSERT_ RC_CONCAT(RC_HASH_TRIE_, _insert_)

#define RC_HASH_TRIE_BLOCKS_MAKE_ RC_CONCAT(RC_HASH_TRIE_BLOCKS_, _make)
#define RC_HASH_TRIE_BLOCKS_RESERVE_ RC_CONCAT(RC_HASH_TRIE_BLOCKS_, _reserve)
#define RC_HASH_TRIE_BLOCKS_RESET_ RC_CONCAT(RC_HASH_TRIE_BLOCKS_, _reset)
#define RC_HASH_TRIE_BLOCKS_RELEASE_ RC_CONCAT(RC_HASH_TRIE_BLOCKS_, _release)
#define RC_HASH_TRIE_BLOCKS_ADD_ RC_CONCAT(RC_HASH_TRIE_BLOCKS_, _add)
#define RC_HASH_TRIE_BLOCKS_REMOVE_ RC_CONCAT(RC_HASH_TRIE_BLOCKS_, _remove)
#define RC_HASH_TRIE_BLOCKS_AT_ RC_CONCAT(RC_HASH_TRIE_BLOCKS_, _at)
#define RC_HASH_TRIE_BLOCKS_AT_CONST_ RC_CONCAT(RC_HASH_TRIE_BLOCKS_, _at_const)

// ---- types ----

typedef struct RC_HASH_TRIE_NODE_ {
    RC_HASH_TRIE_KEY_TYPE key;
#ifdef RC_HASH_TRIE_VALUE_TYPE
    RC_HASH_TRIE_VALUE_TYPE value;
#endif
    uint32_t hash_;   // the key's hash
    uint32_t child_;  // empty, a leaf, or the child block's index plus RC_HASH_TRIE_BLOCK_LINK_
} RC_HASH_TRIE_NODE_;

typedef struct RC_HASH_TRIE_BLOCK_ {
    RC_HASH_TRIE_NODE_ nodes[RC_HASH_TRIE_WIDTH];
} RC_HASH_TRIE_BLOCK_;

#define RC_POOL_TYPE RC_HASH_TRIE_BLOCK_
#define RC_POOL_NAME RC_CONCAT(hash_trie_, RC_CONCAT(RC_HASH_TRIE_NAME, _block_))
#include "richc/pool.template.h"

// The blocks every trie on it lives in. A block emptied by remove or release is reused.
typedef struct RC_HASH_TRIE_POOL_ {
    RC_HASH_TRIE_BLOCKS_ blocks_;
} RC_HASH_TRIE_POOL_;

typedef struct RC_HASH_TRIE_ {
    uint32_t root_;  // the root block's index plus one; 0 before the first add
} RC_HASH_TRIE_;

// For walks, here and in hash_trie_foreach.template.h: the width, and the most blocks a walk can
// have waiting. Only the first levels' blocks can hold more than one key (see How it works), and
// each of those leaves at most width - 1 children waiting while the walk goes down another.
enum {
    RC_HASH_TRIE_WIDTH_CONST_ = RC_HASH_TRIE_WIDTH,
    RC_HASH_TRIE_WALK_MAX_ = (RC_HASH_TRIE_WIDTH - 1) * (int)RC_HASH_TRIE_LEVELS_ + 1
};

// ---- the pool ----

// An empty pool with room for capacity blocks. Only needed to reserve room up front.
static inline RC_HASH_TRIE_POOL_ RC_HASH_TRIE_POOL_MAKE_(uint32_t capacity, rc_arena *arena)
{
    return (RC_HASH_TRIE_POOL_) {.blocks_ = RC_HASH_TRIE_BLOCKS_MAKE_(capacity, arena)};
}

// Make room for at least capacity blocks; never shrinks.
static inline void RC_HASH_TRIE_POOL_RESERVE_(RC_HASH_TRIE_POOL_ *pool, uint32_t capacity, rc_arena *arena)
{
    RC_HASH_TRIE_BLOCKS_RESERVE_(&pool->blocks_, capacity, arena);
}

// Empty the pool, keeping its memory. Every trie on it must be set back to {0}.
static inline void RC_HASH_TRIE_POOL_RESET_(RC_HASH_TRIE_POOL_ *pool)
{
    RC_HASH_TRIE_BLOCKS_RESET_(&pool->blocks_);
}

// Free the blocks (if they are the arena's last allocation) and zero the pool.
static inline void RC_HASH_TRIE_POOL_RELEASE_(RC_HASH_TRIE_POOL_ *pool, rc_arena *arena)
{
    RC_HASH_TRIE_BLOCKS_RELEASE_(&pool->blocks_, arena);
}

// ---- internal ----

// A new block of empty slots, reusing a freed one if there is one. May move the blocks, so node
// pointers must be found again afterwards.
static inline uint32_t RC_HASH_TRIE_ADD_BLOCK_(RC_HASH_TRIE_POOL_ *pool, rc_arena *arena)
{
    return RC_HASH_TRIE_BLOCKS_ADD_(&pool->blocks_, (RC_HASH_TRIE_BLOCK_) {0}, arena);
}

// The block at an index; hash_trie_foreach.template.h uses the const one too.
static inline RC_HASH_TRIE_BLOCK_ *RC_HASH_TRIE_BLOCK_AT_(RC_HASH_TRIE_POOL_ *pool, uint32_t index)
{
    return RC_HASH_TRIE_BLOCKS_AT_(&pool->blocks_, index);
}

static inline const RC_HASH_TRIE_BLOCK_ *RC_HASH_TRIE_BLOCK_AT_CONST_(const RC_HASH_TRIE_POOL_ *pool, uint32_t index)
{
    return RC_HASH_TRIE_BLOCKS_AT_CONST_(&pool->blocks_, index);
}

// The node at a node index.
static inline RC_HASH_TRIE_NODE_ *RC_HASH_TRIE_NODE_AT_(RC_HASH_TRIE_POOL_ *pool, uint32_t index)
{
    return &RC_HASH_TRIE_BLOCK_AT_(pool, index / RC_HASH_TRIE_WIDTH)->nodes[index % RC_HASH_TRIE_WIDTH];
}

static inline const RC_HASH_TRIE_NODE_ *RC_HASH_TRIE_NODE_AT_CONST_(const RC_HASH_TRIE_POOL_ *pool, uint32_t index)
{
    return &RC_HASH_TRIE_BLOCK_AT_CONST_(pool, index / RC_HASH_TRIE_WIDTH)->nodes[index % RC_HASH_TRIE_WIDTH];
}

// True when every slot of block is empty.
static inline bool RC_HASH_TRIE_BLOCK_IS_EMPTY_(const RC_HASH_TRIE_BLOCK_ *block)
{
    for (uint32_t slot = 0; slot < RC_HASH_TRIE_WIDTH; slot++) {
        if (block->nodes[slot].child_ != RC_HASH_TRIE_EMPTY_) {
            return false;
        }
    }
    return true;
}

// The node index of key, or RC_INDEX_NONE.
static inline uint32_t RC_HASH_TRIE_PROBE_(RC_HASH_TRIE_ trie, const RC_HASH_TRIE_POOL_ *pool, RC_HASH_TRIE_KEY_TYPE key)
{
    if (trie.root_ == 0) {
        return RC_INDEX_NONE;
    }

    uint32_t hash = (uint32_t)RC_HASH_TRIE_HASH(key);
    uint32_t path = hash;  // rotated so its low bits pick the slot at each level
    uint32_t block = trie.root_ - 1;
    while (true) {
        uint32_t slot = path & RC_HASH_TRIE_SLOT_MASK_;
        const RC_HASH_TRIE_NODE_ *node = &RC_HASH_TRIE_BLOCK_AT_CONST_(pool, block)->nodes[slot];

        if (node->child_ == RC_HASH_TRIE_EMPTY_) {
            return RC_INDEX_NONE;
        }
        if (node->hash_ == hash && RC_HASH_TRIE_EQUAL(node->key, key)) {
            return block * RC_HASH_TRIE_WIDTH + slot;
        }
        if (node->child_ == RC_HASH_TRIE_LEAF_) {
            return RC_INDEX_NONE;
        }
        block = node->child_ - RC_HASH_TRIE_BLOCK_LINK_;
        path = rc_hash_trie_rotate_(path, RC_HASH_TRIE_BITS_);
    }
}

// The node index of key, adding it if it is not there. In a map, replace says whether an existing
// key takes the new value. The first add makes the root block.
static inline uint32_t RC_HASH_TRIE_INSERT_(RC_HASH_TRIE_ *trie, RC_HASH_TRIE_POOL_ *pool, RC_HASH_TRIE_KEY_TYPE key,
#ifdef RC_HASH_TRIE_VALUE_TYPE
                                            RC_HASH_TRIE_VALUE_TYPE value, bool replace,
#endif
                                            rc_arena *arena)
{
    if (trie->root_ == 0) {
        trie->root_ = RC_HASH_TRIE_ADD_BLOCK_(pool, arena) + 1;
    }

    uint32_t hash = (uint32_t)RC_HASH_TRIE_HASH(key);
    uint32_t path = hash;  // rotated so its low bits pick the slot at each level
    uint32_t block = trie->root_ - 1;
    while (true) {
        uint32_t slot = path & RC_HASH_TRIE_SLOT_MASK_;
        RC_HASH_TRIE_NODE_ *node = &RC_HASH_TRIE_BLOCK_AT_(pool, block)->nodes[slot];

        if (node->child_ == RC_HASH_TRIE_EMPTY_) {
            *node = (RC_HASH_TRIE_NODE_) {
                .key = key,
#ifdef RC_HASH_TRIE_VALUE_TYPE
                .value = value,
#endif
                .hash_ = hash,
                .child_ = RC_HASH_TRIE_LEAF_
            };
            return block * RC_HASH_TRIE_WIDTH + slot;
        }
        if (node->hash_ == hash && RC_HASH_TRIE_EQUAL(node->key, key)) {
#ifdef RC_HASH_TRIE_VALUE_TYPE
            if (replace) {
                node->value = value;
            }
#endif
            return block * RC_HASH_TRIE_WIDTH + slot;
        }

        // The slot's key stays, and ours goes a level down, making the child block if need be.
        // Making it may move the blocks, so the link is written through a fresh pointer.
        if (node->child_ == RC_HASH_TRIE_LEAF_) {
            uint32_t child = RC_HASH_TRIE_ADD_BLOCK_(pool, arena);

            RC_HASH_TRIE_BLOCK_AT_(pool, block)->nodes[slot].child_ = child + RC_HASH_TRIE_BLOCK_LINK_;
            block = child;
        }
        else {
            block = node->child_ - RC_HASH_TRIE_BLOCK_LINK_;
        }
        path = rc_hash_trie_rotate_(path, RC_HASH_TRIE_BITS_);
    }
}

// ---- the trie ----

// Give every block of the trie back to the pool, and set the trie back to {0}. Its copies must be
// set back to {0} too.
static inline void RC_HASH_TRIE_RELEASE_(RC_HASH_TRIE_ *trie, RC_HASH_TRIE_POOL_ *pool)
{
    if (trie->root_ == 0) {
        return;
    }

    // Each block's children are read before it is freed, since freeing overwrites it
    uint32_t waiting[RC_HASH_TRIE_WALK_MAX_];
    uint32_t num_waiting = 1;
    waiting[0] = trie->root_ - 1;
    while (num_waiting > 0) {
        num_waiting--;

        uint32_t block = waiting[num_waiting];
        const RC_HASH_TRIE_NODE_ *nodes = RC_HASH_TRIE_BLOCK_AT_CONST_(pool, block)->nodes;
        for (uint32_t slot = 0; slot < RC_HASH_TRIE_WIDTH; slot++) {
            if (nodes[slot].child_ >= RC_HASH_TRIE_BLOCK_LINK_) {
                RC_ASSERT(num_waiting < RC_HASH_TRIE_WALK_MAX_);
                waiting[num_waiting] = nodes[slot].child_ - RC_HASH_TRIE_BLOCK_LINK_;
                num_waiting++;
            }
        }
        RC_HASH_TRIE_BLOCKS_REMOVE_(&pool->blocks_, block);
    }
    *trie = (RC_HASH_TRIE_) {0};
}

// ---- keys ----

// True if key is in the trie.
static inline bool RC_HASH_TRIE_CONTAINS_(RC_HASH_TRIE_ trie, const RC_HASH_TRIE_POOL_ *pool, RC_HASH_TRIE_KEY_TYPE key)
{
    return RC_HASH_TRIE_PROBE_(trie, pool, key) != RC_INDEX_NONE;
}

// The node index of key, or RC_INDEX_NONE.
static inline uint32_t RC_HASH_TRIE_FIND_(RC_HASH_TRIE_ trie, const RC_HASH_TRIE_POOL_ *pool, RC_HASH_TRIE_KEY_TYPE key)
{
    return RC_HASH_TRIE_PROBE_(trie, pool, key);
}

#ifdef RC_HASH_TRIE_VALUE_TYPE

// Add key with value if it is not there, returning its node index either way. An existing key
// keeps its value.
static inline uint32_t RC_HASH_TRIE_ADD_(RC_HASH_TRIE_ *trie, RC_HASH_TRIE_POOL_ *pool, RC_HASH_TRIE_KEY_TYPE key,
                                         RC_HASH_TRIE_VALUE_TYPE value, rc_arena *arena)
{
    return RC_HASH_TRIE_INSERT_(trie, pool, key, value, false, arena);
}

// Add key with value, or give an existing key value, returning its node index.
static inline uint32_t RC_HASH_TRIE_SET_(RC_HASH_TRIE_ *trie, RC_HASH_TRIE_POOL_ *pool, RC_HASH_TRIE_KEY_TYPE key,
                                         RC_HASH_TRIE_VALUE_TYPE value, rc_arena *arena)
{
    return RC_HASH_TRIE_INSERT_(trie, pool, key, value, true, arena);
}

#else

// Add key if it is not there, returning its node index either way.
static inline uint32_t RC_HASH_TRIE_ADD_(RC_HASH_TRIE_ *trie, RC_HASH_TRIE_POOL_ *pool, RC_HASH_TRIE_KEY_TYPE key,
                                         rc_arena *arena)
{
    return RC_HASH_TRIE_INSERT_(trie, pool, key, arena);
}

#endif // RC_HASH_TRIE_VALUE_TYPE

// Remove key: true if it was there.
static inline bool RC_HASH_TRIE_REMOVE_(RC_HASH_TRIE_ trie, RC_HASH_TRIE_POOL_ *pool, RC_HASH_TRIE_KEY_TYPE key)
{
    if (trie.root_ == 0) {
        return false;
    }

    // Find the key's node, and the node linking to its block (none for the root)
    uint32_t hash = (uint32_t)RC_HASH_TRIE_HASH(key);
    uint32_t path = hash;  // rotated so its low bits pick the slot at each level
    uint32_t index = (trie.root_ - 1) * RC_HASH_TRIE_WIDTH + (path & RC_HASH_TRIE_SLOT_MASK_);
    uint32_t parent = RC_INDEX_NONE;
    while (true) {
        const RC_HASH_TRIE_NODE_ *node = RC_HASH_TRIE_NODE_AT_CONST_(pool, index);

        if (node->child_ == RC_HASH_TRIE_EMPTY_) {
            return false;
        }
        if (node->hash_ == hash && RC_HASH_TRIE_EQUAL(node->key, key)) {
            break;
        }
        if (node->child_ == RC_HASH_TRIE_LEAF_) {
            return false;
        }
        path = rc_hash_trie_rotate_(path, RC_HASH_TRIE_BITS_);
        parent = index;
        index = (node->child_ - RC_HASH_TRIE_BLOCK_LINK_) * RC_HASH_TRIE_WIDTH + (path & RC_HASH_TRIE_SLOT_MASK_);
    }

    // While the node has children, its child block's first key moves up into it, and that key's
    // old node is the next to go. Nothing grows, so node pointers stay valid.
    RC_HASH_TRIE_NODE_ *node = RC_HASH_TRIE_NODE_AT_(pool, index);
    while (node->child_ != RC_HASH_TRIE_LEAF_) {
        uint32_t child = node->child_ - RC_HASH_TRIE_BLOCK_LINK_;
        const RC_HASH_TRIE_NODE_ *below = RC_HASH_TRIE_BLOCK_AT_CONST_(pool, child)->nodes;
        uint32_t first = 0;

        while (below[first].child_ == RC_HASH_TRIE_EMPTY_) {
            first++;
            RC_ASSERT(first < RC_HASH_TRIE_WIDTH);
        }
        *node = (RC_HASH_TRIE_NODE_) {
            .key = below[first].key,
#ifdef RC_HASH_TRIE_VALUE_TYPE
            .value = below[first].value,
#endif
            .hash_ = below[first].hash_,
            .child_ = child + RC_HASH_TRIE_BLOCK_LINK_
        };
        parent = index;
        index = child * RC_HASH_TRIE_WIDTH + first;
        node = RC_HASH_TRIE_NODE_AT_(pool, index);
    }

    // A key with no children: its slot empties, and a block below the root left empty is unlinked
    // and freed
    uint32_t block = index / RC_HASH_TRIE_WIDTH;
    node->child_ = RC_HASH_TRIE_EMPTY_;
    if (parent != RC_INDEX_NONE && RC_HASH_TRIE_BLOCK_IS_EMPTY_(RC_HASH_TRIE_BLOCK_AT_CONST_(pool, block))) {
        RC_HASH_TRIE_NODE_AT_(pool, parent)->child_ = RC_HASH_TRIE_LEAF_;
        RC_HASH_TRIE_BLOCKS_REMOVE_(&pool->blocks_, block);
    }
    return true;
}

// The key at a node index, by value. Keys never change once added, so there is no key_set.
static inline RC_HASH_TRIE_KEY_TYPE RC_HASH_TRIE_KEY_GET_(const RC_HASH_TRIE_POOL_ *pool, uint32_t index)
{
    const RC_HASH_TRIE_NODE_ *node = RC_HASH_TRIE_NODE_AT_CONST_(pool, index);

    RC_ASSERT(node->child_ != RC_HASH_TRIE_EMPTY_);
    return node->key;
}

// ---- values (maps only) ----

#ifdef RC_HASH_TRIE_VALUE_TYPE

// The value at a node index, by value.
static inline RC_HASH_TRIE_VALUE_TYPE RC_HASH_TRIE_VALUE_GET_(const RC_HASH_TRIE_POOL_ *pool, uint32_t index)
{
    const RC_HASH_TRIE_NODE_ *node = RC_HASH_TRIE_NODE_AT_CONST_(pool, index);

    RC_ASSERT(node->child_ != RC_HASH_TRIE_EMPTY_);
    return node->value;
}

// Overwrite the value at a node index.
static inline void RC_HASH_TRIE_VALUE_SET_(RC_HASH_TRIE_POOL_ *pool, uint32_t index, RC_HASH_TRIE_VALUE_TYPE value)
{
    RC_HASH_TRIE_NODE_ *node = RC_HASH_TRIE_NODE_AT_(pool, index);

    RC_ASSERT(node->child_ != RC_HASH_TRIE_EMPTY_);
    node->value = value;
}

// A pointer to the value at a node index. Anything that grows the pool may move it.
static inline RC_HASH_TRIE_VALUE_TYPE *RC_HASH_TRIE_VALUE_AT_(RC_HASH_TRIE_POOL_ *pool, uint32_t index)
{
    RC_HASH_TRIE_NODE_ *node = RC_HASH_TRIE_NODE_AT_(pool, index);

    RC_ASSERT(node->child_ != RC_HASH_TRIE_EMPTY_);
    return &node->value;
}

#endif // RC_HASH_TRIE_VALUE_TYPE

// ---- cleanup ----

#undef RC_HASH_TRIE_POOL_MAKE_
#undef RC_HASH_TRIE_POOL_RESERVE_
#undef RC_HASH_TRIE_POOL_RESET_
#undef RC_HASH_TRIE_POOL_RELEASE_
#undef RC_HASH_TRIE_RELEASE_
#undef RC_HASH_TRIE_CONTAINS_
#undef RC_HASH_TRIE_FIND_
#undef RC_HASH_TRIE_ADD_
#undef RC_HASH_TRIE_SET_
#undef RC_HASH_TRIE_REMOVE_
#undef RC_HASH_TRIE_KEY_GET_
#undef RC_HASH_TRIE_VALUE_GET_
#undef RC_HASH_TRIE_VALUE_SET_
#undef RC_HASH_TRIE_VALUE_AT_

#undef RC_HASH_TRIE_ADD_BLOCK_
#undef RC_HASH_TRIE_BLOCK_AT_
#undef RC_HASH_TRIE_BLOCK_AT_CONST_
#undef RC_HASH_TRIE_NODE_AT_
#undef RC_HASH_TRIE_NODE_AT_CONST_
#undef RC_HASH_TRIE_BLOCK_IS_EMPTY_
#undef RC_HASH_TRIE_PROBE_
#undef RC_HASH_TRIE_INSERT_

#undef RC_HASH_TRIE_BLOCKS_MAKE_
#undef RC_HASH_TRIE_BLOCKS_RESERVE_
#undef RC_HASH_TRIE_BLOCKS_RESET_
#undef RC_HASH_TRIE_BLOCKS_RELEASE_
#undef RC_HASH_TRIE_BLOCKS_ADD_
#undef RC_HASH_TRIE_BLOCKS_REMOVE_
#undef RC_HASH_TRIE_BLOCKS_AT_
#undef RC_HASH_TRIE_BLOCKS_AT_CONST_

#undef RC_HASH_TRIE_
#undef RC_HASH_TRIE_POOL_
#undef RC_HASH_TRIE_NODE_
#undef RC_HASH_TRIE_BLOCK_
#undef RC_HASH_TRIE_BLOCKS_
#undef RC_HASH_TRIE_WIDTH_CONST_
#undef RC_HASH_TRIE_WALK_MAX_
#undef RC_HASH_TRIE_SLOT_MASK_
#undef RC_HASH_TRIE_LEVELS_
#undef RC_HASH_TRIE_BITS_

#undef RC_HASH_TRIE_NAME
#undef RC_HASH_TRIE_WIDTH
#undef RC_HASH_TRIE_EQUAL
#undef RC_HASH_TRIE_HASH
#undef RC_HASH_TRIE_VALUE_TYPE
#undef RC_HASH_TRIE_KEY_TYPE
