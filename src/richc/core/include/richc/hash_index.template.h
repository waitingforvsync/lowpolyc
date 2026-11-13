// hash_index.template.h
//
// Template for typed lookups through a hash index (hash_index.h), reading the keys from your
// data. It makes no type of its own: the functions work on a plain rc_hash_index.
//
//   RC_HASH_INDEX_KEY_TYPE             the key type (required)
//   RC_HASH_INDEX_DATA_TYPE            what the keys are read from, passed by value, such as a
//                                      view or a small struct of views (required)
//   RC_HASH_INDEX_KEY_AT(data, index)  the key at an index of data (required)
//   RC_HASH_INDEX_HASH(key)            the key's hash, converted to uint32_t (required)
//   RC_HASH_INDEX_EQUAL(a, b)          key equality; defaults to (a) == (b)
//   RC_HASH_INDEX_NAME                 the name suffix; defaults to RC_HASH_INDEX_KEY_TYPE, which
//                                      must then be one identifier
//
// The functions are rc_hash_index_<name>_find, _contains, _find_or_add, _add, _remove and _move.
//
// The data is whatever reaches your keys. With one array of {key, value} structs, it is that
// array's view and KEY_AT reads .key. With keys and values in separate arrays, it is just the keys'
// view, and the values never come near the table.
//
// Gathering unique positions, with both made big enough up front to need no arena:
//
//   uint32_t i = rc_hash_index_vec3_find_or_add(&table, unique.view, pos, unique.num, NULL);
//   if (i == unique.num) {
//       rc_array_vec3_push(&unique, pos, NULL);
//   }
//
// Removing a key while keeping the arrays dense, by moving the last element into its place:
//
//   uint32_t i = rc_hash_index_vec3_remove(&table, unique.view, pos);
//   uint32_t last = unique.num - 1;
//   if (i != last) {
//       rc_hash_index_vec3_move(&table, unique.view, last, i);
//       rc_array_vec3_set(&unique, i, rc_array_vec3_get(&unique, last));
//   }
//   rc_array_vec3_pop(&unique);

#include "richc/hash_index.h"

// The fallbacks keep IntelliSense happy when it parses this file on its own
#ifndef RC_HASH_INDEX_KEY_TYPE
#  define RC_HASH_INDEX_KEY_TYPE int
#  error "RC_HASH_INDEX_KEY_TYPE must be defined before including richc/hash_index.template.h"
#endif

#ifndef RC_HASH_INDEX_DATA_TYPE
#  define RC_HASH_INDEX_DATA_TYPE const int *
#  error "RC_HASH_INDEX_DATA_TYPE must be defined before including richc/hash_index.template.h"
#endif

#ifndef RC_HASH_INDEX_KEY_AT
#  define RC_HASH_INDEX_KEY_AT(data, index) ((data)[index])
#  error "RC_HASH_INDEX_KEY_AT must be defined before including richc/hash_index.template.h"
#endif

#ifndef RC_HASH_INDEX_HASH
#  define RC_HASH_INDEX_HASH(key) (key)
#  error "RC_HASH_INDEX_HASH must be defined before including richc/hash_index.template.h"
#endif

#ifndef RC_HASH_INDEX_EQUAL
#  define RC_HASH_INDEX_EQUAL(a, b) ((a) == (b))
#endif

#ifndef RC_HASH_INDEX_NAME
#  define RC_HASH_INDEX_NAME RC_HASH_INDEX_KEY_TYPE
#endif

#define RC_HASH_INDEX_TYPED_ RC_CONCAT(rc_hash_index_, RC_HASH_INDEX_NAME)

#define RC_HASH_INDEX_FIND_ RC_CONCAT(RC_HASH_INDEX_TYPED_, _find)
#define RC_HASH_INDEX_CONTAINS_ RC_CONCAT(RC_HASH_INDEX_TYPED_, _contains)
#define RC_HASH_INDEX_FIND_OR_ADD_ RC_CONCAT(RC_HASH_INDEX_TYPED_, _find_or_add)
#define RC_HASH_INDEX_ADD_ RC_CONCAT(RC_HASH_INDEX_TYPED_, _add)
#define RC_HASH_INDEX_REMOVE_ RC_CONCAT(RC_HASH_INDEX_TYPED_, _remove)
#define RC_HASH_INDEX_MOVE_ RC_CONCAT(RC_HASH_INDEX_TYPED_, _move)
#define RC_HASH_INDEX_SEARCH_ RC_CONCAT(RC_HASH_INDEX_TYPED_, _search_)

// ---- internal ----

// The index of key under hash, or RC_INDEX_NONE.
static inline uint32_t RC_HASH_INDEX_SEARCH_(rc_hash_index table, RC_HASH_INDEX_DATA_TYPE data, RC_HASH_INDEX_KEY_TYPE key,
                                             uint32_t hash)
{
    rc_hash_index_probe probe = rc_hash_index_probe_make(table, hash);
    uint32_t index = rc_hash_index_probe_next(table, &probe);

    while (index != RC_INDEX_NONE && !RC_HASH_INDEX_EQUAL(RC_HASH_INDEX_KEY_AT(data, index), key)) {
        index = rc_hash_index_probe_next(table, &probe);
    }
    return index;
}

// ---- lookups ----

// The index of key in data, or RC_INDEX_NONE. With several entries for key, any one of them.
static inline uint32_t RC_HASH_INDEX_FIND_(rc_hash_index table, RC_HASH_INDEX_DATA_TYPE data, RC_HASH_INDEX_KEY_TYPE key)
{
    return RC_HASH_INDEX_SEARCH_(table, data, key, (uint32_t)RC_HASH_INDEX_HASH(key));
}

// True if key has an entry.
static inline bool RC_HASH_INDEX_CONTAINS_(rc_hash_index table, RC_HASH_INDEX_DATA_TYPE data, RC_HASH_INDEX_KEY_TYPE key)
{
    return RC_HASH_INDEX_FIND_(table, data, key) != RC_INDEX_NONE;
}

// ---- changes ----

// The index of key in data, or if it has no entry, new_index after adding one for it there. You
// then put key at new_index, typically by pushing it.
static inline uint32_t RC_HASH_INDEX_FIND_OR_ADD_(rc_hash_index *table, RC_HASH_INDEX_DATA_TYPE data,
                                                  RC_HASH_INDEX_KEY_TYPE key, uint32_t new_index, rc_arena *arena)
{
    RC_ASSERT(table);

    uint32_t hash = (uint32_t)RC_HASH_INDEX_HASH(key);
    uint32_t index = RC_HASH_INDEX_SEARCH_(*table, data, key, hash);
    if (index != RC_INDEX_NONE) {
        return index;
    }
    rc_hash_index_add(table, hash, new_index, arena);
    return new_index;
}

// Add an entry for the key at an index of data, even if that key already has one.
static inline void RC_HASH_INDEX_ADD_(rc_hash_index *table, RC_HASH_INDEX_DATA_TYPE data, uint32_t index,
                                      rc_arena *arena)
{
    rc_hash_index_add(table, (uint32_t)RC_HASH_INDEX_HASH(RC_HASH_INDEX_KEY_AT(data, index)), index, arena);
}

// Remove key's entry, returning the index it had, or RC_INDEX_NONE if it had none.
// data is left alone.
static inline uint32_t RC_HASH_INDEX_REMOVE_(rc_hash_index *table, RC_HASH_INDEX_DATA_TYPE data,
                                             RC_HASH_INDEX_KEY_TYPE key)
{
    RC_ASSERT(table);

    uint32_t hash = (uint32_t)RC_HASH_INDEX_HASH(key);
    uint32_t index = RC_HASH_INDEX_SEARCH_(*table, data, key, hash);
    if (index != RC_INDEX_NONE) {
        rc_hash_index_remove(table, hash, index);
    }
    return index;
}

// Point the entry for the key at index from of data at index to instead, before you move the key
// there: true if it had one. data must still hold the key at from.
static inline bool RC_HASH_INDEX_MOVE_(rc_hash_index *table, RC_HASH_INDEX_DATA_TYPE data, uint32_t from, uint32_t to)
{
    return rc_hash_index_move(table, (uint32_t)RC_HASH_INDEX_HASH(RC_HASH_INDEX_KEY_AT(data, from)), from, to);
}

// ---- cleanup ----

#undef RC_HASH_INDEX_FIND_
#undef RC_HASH_INDEX_CONTAINS_
#undef RC_HASH_INDEX_FIND_OR_ADD_
#undef RC_HASH_INDEX_ADD_
#undef RC_HASH_INDEX_REMOVE_
#undef RC_HASH_INDEX_MOVE_
#undef RC_HASH_INDEX_SEARCH_
#undef RC_HASH_INDEX_TYPED_

#undef RC_HASH_INDEX_NAME
#undef RC_HASH_INDEX_EQUAL
#undef RC_HASH_INDEX_HASH
#undef RC_HASH_INDEX_KEY_AT
#undef RC_HASH_INDEX_DATA_TYPE
#undef RC_HASH_INDEX_KEY_TYPE
