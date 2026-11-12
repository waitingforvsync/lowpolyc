// array.template.h
//
// Template for one element type's read-only view, mutable span and growable arena-backed array.
// Define the control macros, include this, and it generates the types and functions, then
// #undefs everything so it can be included again for another type.
//
//   RC_ARRAY_TYPE          the element type (required)
//   RC_ARRAY_NAME          the name suffix; defaults to RC_ARRAY_TYPE, which must then be one
//                          identifier
//   RC_ARRAY_DECLARE_ONLY  generate only the types
//   RC_ARRAY_IMPL_ONLY     generate only the functions, after a DECLARE_ONLY pass
//
// The types are rc_view_<name> {data, num}, rc_span_<name> {data, num} and rc_array_<name>
// {data, num, cap}. The span and array overlay their smaller relatives in an anonymous union, so
// arr.span, arr.view and span.view convert with no call and full type safety.
//
// An element type holding a view of itself needs two passes: DECLARE_ONLY while it is still
// incomplete, then IMPL_ONLY once it is defined.
//
//   typedef struct value value;
//   #define RC_ARRAY_TYPE value
//   #define RC_ARRAY_DECLARE_ONLY
//   #include "richc/array.template.h"
//   struct value { rc_view_value children; };
//   #define RC_ARRAY_TYPE value
//   #define RC_ARRAY_IMPL_ONLY
//   #include "richc/array.template.h"
//
// Everything that may reallocate takes the arena last. It may be NULL when no growth is needed.
// reserve, make, make_copy, from_view and resize allocate exactly what they are asked for; the
// incremental operations (push, append, insert) grow to the larger of double the capacity, the
// need, or 8, so they stay amortised O(1).

// ---- shared by every instantiation ----

#ifndef RC_ARRAY_TEMPLATE_H_
#define RC_ARRAY_TEMPLATE_H_

#include "richc/arena.h"
#include "richc/mem.h"

// Element i of any view, span or array, as an lvalue. Asserts i < c.num. It is not type-checked
// and exists only for convenience; richc and lowpoly use the typed accessors instead.
#define RC_AT(c, i) (*(RC_ASSERT((uint32_t)(i) < (c).num), (c).data + (uint32_t)(i)))

// Brace initialisers for a view or span of a C array, for declarations only. Never pass a
// pointer: the count comes from sizeof.
#define RC_VIEW(arr) {.data = (arr), .num = (uint32_t)(sizeof(arr) / sizeof((arr)[0]))}
#define RC_SPAN(arr) {.data = (arr), .num = (uint32_t)(sizeof(arr) / sizeof((arr)[0]))}

#endif // RC_ARRAY_TEMPLATE_H_

// ---- per-type names ----

// The fallback type keeps IntelliSense happy when it parses this file on its own
#ifndef RC_ARRAY_TYPE
#  define RC_ARRAY_TYPE int
#  error "RC_ARRAY_TYPE must be defined before including richc/array.template.h"
#endif

#ifndef RC_ARRAY_NAME
#  define RC_ARRAY_NAME RC_ARRAY_TYPE
#endif

#define RC_ARRAY_ RC_CONCAT(rc_array_, RC_ARRAY_NAME)
#define RC_SPAN_ RC_CONCAT(rc_span_, RC_ARRAY_NAME)
#define RC_VIEW_ RC_CONCAT(rc_view_, RC_ARRAY_NAME)

#ifndef RC_ARRAY_DECLARE_ONLY

#define RC_ARRAY_MAKE_ RC_CONCAT(RC_ARRAY_, _make)
#define RC_ARRAY_MAKE_COPY_ RC_CONCAT(RC_ARRAY_, _make_copy)
#define RC_ARRAY_FROM_VIEW_ RC_CONCAT(RC_ARRAY_, _from_view)
#define RC_ARRAY_GET_ RC_CONCAT(RC_ARRAY_, _get)
#define RC_ARRAY_SET_ RC_CONCAT(RC_ARRAY_, _set)
#define RC_ARRAY_AT_ RC_CONCAT(RC_ARRAY_, _at)
#define RC_ARRAY_LAST_AT_ RC_CONCAT(RC_ARRAY_, _last_at)
#define RC_ARRAY_IS_VALID_ RC_CONCAT(RC_ARRAY_, _is_valid)
#define RC_ARRAY_IS_EMPTY_ RC_CONCAT(RC_ARRAY_, _is_empty)
#define RC_ARRAY_RESERVE_ RC_CONCAT(RC_ARRAY_, _reserve)
#define RC_ARRAY_GROW_ RC_CONCAT(RC_ARRAY_, _grow_)
#define RC_ARRAY_RESIZE_ RC_CONCAT(RC_ARRAY_, _resize)
#define RC_ARRAY_RESIZE_ZERO_ RC_CONCAT(RC_ARRAY_, _resize_zero)
#define RC_ARRAY_RESET_ RC_CONCAT(RC_ARRAY_, _reset)
#define RC_ARRAY_RELEASE_ RC_CONCAT(RC_ARRAY_, _release)
#define RC_ARRAY_PUSH_ RC_CONCAT(RC_ARRAY_, _push)
#define RC_ARRAY_PUSH_N_ RC_CONCAT(RC_ARRAY_, _push_n)
#define RC_ARRAY_PUSH_N_ZERO_ RC_CONCAT(RC_ARRAY_, _push_n_zero)
#define RC_ARRAY_POP_ RC_CONCAT(RC_ARRAY_, _pop)
#define RC_ARRAY_POP_N_ RC_CONCAT(RC_ARRAY_, _pop_n)
#define RC_ARRAY_APPEND_ RC_CONCAT(RC_ARRAY_, _append)
#define RC_ARRAY_INSERT_ RC_CONCAT(RC_ARRAY_, _insert)
#define RC_ARRAY_INSERT_N_ RC_CONCAT(RC_ARRAY_, _insert_n)
#define RC_ARRAY_INSERT_N_ZERO_ RC_CONCAT(RC_ARRAY_, _insert_n_zero)
#define RC_ARRAY_REMOVE_ RC_CONCAT(RC_ARRAY_, _remove)
#define RC_ARRAY_REMOVE_N_ RC_CONCAT(RC_ARRAY_, _remove_n)

#define RC_SPAN_MAKE_ RC_CONCAT(RC_SPAN_, _make)
#define RC_SPAN_MAKE_ZERO_ RC_CONCAT(RC_SPAN_, _make_zero)
#define RC_SPAN_FROM_PTR_ RC_CONCAT(RC_SPAN_, _from_ptr)
#define RC_SPAN_IS_VALID_ RC_CONCAT(RC_SPAN_, _is_valid)
#define RC_SPAN_IS_EMPTY_ RC_CONCAT(RC_SPAN_, _is_empty)
#define RC_SPAN_FIRST_ RC_CONCAT(RC_SPAN_, _first)
#define RC_SPAN_LAST_ RC_CONCAT(RC_SPAN_, _last)
#define RC_SPAN_DROP_FIRST_ RC_CONCAT(RC_SPAN_, _drop_first)
#define RC_SPAN_DROP_LAST_ RC_CONCAT(RC_SPAN_, _drop_last)
#define RC_SPAN_SUBSPAN_ RC_CONCAT(RC_SPAN_, _subspan)
#define RC_SPAN_GET_ RC_CONCAT(RC_SPAN_, _get)
#define RC_SPAN_SET_ RC_CONCAT(RC_SPAN_, _set)
#define RC_SPAN_AT_ RC_CONCAT(RC_SPAN_, _at)
#define RC_SPAN_LAST_AT_ RC_CONCAT(RC_SPAN_, _last_at)
#define RC_SPAN_REVERSE_ RC_CONCAT(RC_SPAN_, _reverse)
#define RC_SPAN_ROTATE_ RC_CONCAT(RC_SPAN_, _rotate)
#define RC_SPAN_SWAP_RANGES_ RC_CONCAT(RC_SPAN_, _swap_ranges_)

#define RC_VIEW_FROM_PTR_ RC_CONCAT(RC_VIEW_, _from_ptr)
#define RC_VIEW_IS_VALID_ RC_CONCAT(RC_VIEW_, _is_valid)
#define RC_VIEW_IS_EMPTY_ RC_CONCAT(RC_VIEW_, _is_empty)
#define RC_VIEW_FIRST_ RC_CONCAT(RC_VIEW_, _first)
#define RC_VIEW_LAST_ RC_CONCAT(RC_VIEW_, _last)
#define RC_VIEW_DROP_FIRST_ RC_CONCAT(RC_VIEW_, _drop_first)
#define RC_VIEW_DROP_LAST_ RC_CONCAT(RC_VIEW_, _drop_last)
#define RC_VIEW_SUBVIEW_ RC_CONCAT(RC_VIEW_, _subview)
#define RC_VIEW_GET_ RC_CONCAT(RC_VIEW_, _get)
#define RC_VIEW_AT_ RC_CONCAT(RC_VIEW_, _at)
#define RC_VIEW_LAST_AT_ RC_CONCAT(RC_VIEW_, _last_at)

#endif // RC_ARRAY_DECLARE_ONLY

// ---- types ----

#ifndef RC_ARRAY_IMPL_ONLY

// A read-only run of elements.
typedef struct RC_VIEW_ {
    const RC_ARRAY_TYPE *data;
    uint32_t num;
} RC_VIEW_;

// A mutable run of elements, which is also its own read-only view.
typedef struct RC_SPAN_ {
    union {
        RC_VIEW_ view;
        struct {
            RC_ARRAY_TYPE *data;
            uint32_t num;
        };
    };
} RC_SPAN_;

// A growable, arena-backed array, which is also its own span and view.
typedef struct RC_ARRAY_ {
    union {
        RC_SPAN_ span;
        RC_VIEW_ view;
        struct {
            RC_ARRAY_TYPE *data;
            uint32_t num;
        };
    };
    uint32_t cap;
} RC_ARRAY_;

#endif // RC_ARRAY_IMPL_ONLY

#ifndef RC_ARRAY_DECLARE_ONLY

// ---- predicates ----

// True once the array has a buffer.
static inline bool RC_ARRAY_IS_VALID_(const RC_ARRAY_ *array)
{
    RC_ASSERT(array);
    return array->data != NULL;
}

// True when the array has no elements.
static inline bool RC_ARRAY_IS_EMPTY_(const RC_ARRAY_ *array)
{
    RC_ASSERT(array);
    return array->num == 0;
}

// True when the span points somewhere.
static inline bool RC_SPAN_IS_VALID_(RC_SPAN_ span)
{
    return span.data != NULL;
}

// True when the span has no elements.
static inline bool RC_SPAN_IS_EMPTY_(RC_SPAN_ span)
{
    return span.num == 0;
}

// True when the view points somewhere.
static inline bool RC_VIEW_IS_VALID_(RC_VIEW_ view)
{
    return view.data != NULL;
}

// True when the view has no elements.
static inline bool RC_VIEW_IS_EMPTY_(RC_VIEW_ view)
{
    return view.num == 0;
}

// ---- array ----

// Make the buffer hold at least capacity elements, exactly; never shrinks it. Panics (in release
// too) if the size in bytes would overflow 32 bits.
static inline void RC_ARRAY_RESERVE_(RC_ARRAY_ *array, uint32_t capacity, rc_arena *arena)
{
    RC_ASSERT(array);
    if (capacity <= array->cap) {
        return;
    }
    RC_PANIC((uint64_t)capacity * sizeof(RC_ARRAY_TYPE) <= UINT32_MAX);
    RC_ASSERT(arena);

    uint32_t old_size = (uint32_t)(array->cap * sizeof(RC_ARRAY_TYPE));
    uint32_t new_size = (uint32_t)(capacity * sizeof(RC_ARRAY_TYPE));
    array->data = (RC_ARRAY_TYPE *)rc_arena_realloc(arena, array->data, old_size, new_size);
    array->cap = capacity;
}

// Make room for n more elements, growing to the larger of double the capacity (as far as a 32-bit
// byte size allows), the need, or 8. Panics (in release too) if the count would wrap: we would
// skip the growth and write past the end.
static inline void RC_ARRAY_GROW_(RC_ARRAY_ *array, uint32_t n, rc_arena *arena)
{
    RC_ASSERT(array);
    RC_PANIC(n <= UINT32_MAX - array->num);

    uint32_t need = array->num + n;
    if (need <= array->cap) {
        return;
    }

    uint32_t max_cap = (uint32_t)(UINT32_MAX / sizeof(RC_ARRAY_TYPE));
    uint32_t doubled = array->cap > max_cap / 2 ? max_cap : array->cap * 2;
    uint32_t new_cap = need > doubled ? need : doubled;
    if (new_cap < 8) {
        new_cap = 8;
    }
    RC_ARRAY_RESERVE_(array, new_cap, arena);
}

// An empty array with room for initial_capacity elements. Capacity 0 allocates nothing, so the
// arena may then be NULL.
static inline RC_ARRAY_ RC_ARRAY_MAKE_(uint32_t initial_capacity, rc_arena *arena)
{
    RC_ARRAY_ array = {0};

    RC_ARRAY_RESERVE_(&array, initial_capacity, arena);
    return array;
}

// A copy of view, with room for at least minimum_capacity elements.
static inline RC_ARRAY_ RC_ARRAY_FROM_VIEW_(RC_VIEW_ view, uint32_t minimum_capacity, rc_arena *arena)
{
    RC_ASSERT(RC_VIEW_IS_VALID_(view) || RC_VIEW_IS_EMPTY_(view));

    RC_ARRAY_ array = RC_ARRAY_MAKE_(view.num > minimum_capacity ? view.num : minimum_capacity, arena);
    if (view.num > 0) {
        rc_memcpy(array.data, view.data, (uint32_t)(view.num * sizeof(RC_ARRAY_TYPE)));
    }
    array.num = view.num;
    return array;
}

// A copy of src, with room for at least minimum_capacity elements: from_view on src's own view.
static inline RC_ARRAY_ RC_ARRAY_MAKE_COPY_(const RC_ARRAY_ *src, uint32_t minimum_capacity, rc_arena *arena)
{
    RC_ASSERT(src);
    return RC_ARRAY_FROM_VIEW_(src->view, minimum_capacity, arena);
}

// Element index by value.
static inline RC_ARRAY_TYPE RC_ARRAY_GET_(const RC_ARRAY_ *array, uint32_t index)
{
    RC_ASSERT(RC_ARRAY_IS_VALID_(array));
    RC_ASSERT(index < array->num);
    return array->data[index];
}

// Overwrite element index.
static inline void RC_ARRAY_SET_(RC_ARRAY_ *array, uint32_t index, RC_ARRAY_TYPE value)
{
    RC_ASSERT(RC_ARRAY_IS_VALID_(array));
    RC_ASSERT(index < array->num);
    array->data[index] = value;
}

// A pointer to element index. Anything that grows the array may move it.
static inline RC_ARRAY_TYPE *RC_ARRAY_AT_(RC_ARRAY_ *array, uint32_t index)
{
    RC_ASSERT(RC_ARRAY_IS_VALID_(array));
    RC_ASSERT(index < array->num);
    return &array->data[index];
}

// A pointer to the last element. Anything that grows the array may move it.
static inline RC_ARRAY_TYPE *RC_ARRAY_LAST_AT_(RC_ARRAY_ *array)
{
    RC_ASSERT(RC_ARRAY_IS_VALID_(array));
    RC_ASSERT(!RC_ARRAY_IS_EMPTY_(array));
    return &array->data[array->num - 1];
}

// Set the element count to num, returning a span of the whole array.
// New elements are not initialised.
static inline RC_SPAN_ RC_ARRAY_RESIZE_(RC_ARRAY_ *array, uint32_t num, rc_arena *arena)
{
    RC_ARRAY_RESERVE_(array, num, arena);
    array->num = num;
    return array->span;
}

// Like resize, but new elements are zeroed.
static inline RC_SPAN_ RC_ARRAY_RESIZE_ZERO_(RC_ARRAY_ *array, uint32_t num, rc_arena *arena)
{
    RC_ASSERT(array);

    uint32_t old_num = array->num;
    RC_ARRAY_RESERVE_(array, num, arena);
    if (num > old_num) {
        rc_memset(array->data + old_num, 0, (uint32_t)((num - old_num) * sizeof(RC_ARRAY_TYPE)));
    }
    array->num = num;
    return array->span;
}

// Remove every element, keeping the buffer.
static inline void RC_ARRAY_RESET_(RC_ARRAY_ *array)
{
    RC_ASSERT(array);
    array->num = 0;
}

// Free the buffer (if it is the arena's last allocation) and zero the array.
static inline void RC_ARRAY_RELEASE_(RC_ARRAY_ *array, rc_arena *arena)
{
    RC_ASSERT(array);
    if (array->data) {
        rc_arena_free(arena, array->data, (uint32_t)(array->cap * sizeof(RC_ARRAY_TYPE)));
    }
    *array = (RC_ARRAY_) {0};
}

// Append value, returning its index.
static inline uint32_t RC_ARRAY_PUSH_(RC_ARRAY_ *array, RC_ARRAY_TYPE value, rc_arena *arena)
{
    RC_ASSERT(array);

    uint32_t index = array->num;
    RC_ARRAY_GROW_(array, 1, arena);
    array->data[index] = value;
    array->num = index + 1;
    return index;
}

// Append n uninitialised elements, returning the index of the first.
static inline uint32_t RC_ARRAY_PUSH_N_(RC_ARRAY_ *array, uint32_t n, rc_arena *arena)
{
    RC_ASSERT(array);

    uint32_t index = array->num;
    RC_ARRAY_GROW_(array, n, arena);
    array->num = index + n;
    return index;
}

// Append n zeroed elements, returning the index of the first.
static inline uint32_t RC_ARRAY_PUSH_N_ZERO_(RC_ARRAY_ *array, uint32_t n, rc_arena *arena)
{
    RC_ASSERT(array);

    uint32_t index = array->num;
    RC_ARRAY_GROW_(array, n, arena);
    if (n > 0) {
        rc_memset(array->data + index, 0, (uint32_t)(n * sizeof(RC_ARRAY_TYPE)));
    }
    array->num = index + n;
    return index;
}

// Remove the last element and return it.
static inline RC_ARRAY_TYPE RC_ARRAY_POP_(RC_ARRAY_ *array)
{
    RC_ASSERT(RC_ARRAY_IS_VALID_(array));
    RC_ASSERT(!RC_ARRAY_IS_EMPTY_(array));
    array->num--;
    return array->data[array->num];
}

// Remove the last n elements.
static inline void RC_ARRAY_POP_N_(RC_ARRAY_ *array, uint32_t n)
{
    RC_ASSERT(array);
    RC_ASSERT(n <= array->num);
    array->num -= n;
}

// Append a copy of view, returning the index of its first element.
static inline uint32_t RC_ARRAY_APPEND_(RC_ARRAY_ *array, RC_VIEW_ view, rc_arena *arena)
{
    RC_ASSERT(array);
    RC_ASSERT(RC_VIEW_IS_VALID_(view) || RC_VIEW_IS_EMPTY_(view));

    uint32_t index = array->num;
    if (view.num > 0) {
        RC_ARRAY_GROW_(array, view.num, arena);
        rc_memcpy(array->data + index, view.data, (uint32_t)(view.num * sizeof(RC_ARRAY_TYPE)));
        array->num = index + view.num;
    }
    return index;
}

// Insert value at index, moving the elements from there up by one.
static inline void RC_ARRAY_INSERT_(RC_ARRAY_ *array, uint32_t index, RC_ARRAY_TYPE value, rc_arena *arena)
{
    RC_ASSERT(array);
    RC_ASSERT(index <= array->num);
    RC_ARRAY_GROW_(array, 1, arena);

    uint32_t tail_size = (uint32_t)((array->num - index) * sizeof(RC_ARRAY_TYPE));
    rc_memmove(array->data + index + 1, array->data + index, tail_size);
    array->data[index] = value;
    array->num++;
}

// Insert n uninitialised elements at index, moving the elements from there up.
static inline void RC_ARRAY_INSERT_N_(RC_ARRAY_ *array, uint32_t index, uint32_t n, rc_arena *arena)
{
    RC_ASSERT(array);
    RC_ASSERT(index <= array->num);
    if (n == 0) {
        return;
    }

    RC_ARRAY_GROW_(array, n, arena);

    uint32_t tail_size = (uint32_t)((array->num - index) * sizeof(RC_ARRAY_TYPE));
    rc_memmove(array->data + index + n, array->data + index, tail_size);
    array->num += n;
}

// Like insert_n, but the new elements are zeroed.
static inline void RC_ARRAY_INSERT_N_ZERO_(RC_ARRAY_ *array, uint32_t index, uint32_t n, rc_arena *arena)
{
    RC_ARRAY_INSERT_N_(array, index, n, arena);
    if (n > 0) {
        rc_memset(array->data + index, 0, (uint32_t)(n * sizeof(RC_ARRAY_TYPE)));
    }
}

// Remove the element at index, moving the ones after it down.
static inline void RC_ARRAY_REMOVE_(RC_ARRAY_ *array, uint32_t index)
{
    RC_ASSERT(RC_ARRAY_IS_VALID_(array));
    RC_ASSERT(index < array->num);

    uint32_t tail_size = (uint32_t)((array->num - index - 1) * sizeof(RC_ARRAY_TYPE));
    rc_memmove(array->data + index, array->data + index + 1, tail_size);
    array->num--;
}

// Remove n elements from index, moving the ones after them down.
static inline void RC_ARRAY_REMOVE_N_(RC_ARRAY_ *array, uint32_t index, uint32_t n)
{
    RC_ASSERT(array);
    RC_ASSERT(index <= array->num && n <= array->num - index);
    if (n == 0) {
        return;
    }

    uint32_t tail_size = (uint32_t)((array->num - index - n) * sizeof(RC_ARRAY_TYPE));
    rc_memmove(array->data + index, array->data + index + n, tail_size);
    array->num -= n;
}

// ---- span ----

// A span of n uninitialised elements, allocated from the arena. n of 0 allocates nothing, so the
// arena may then be NULL. Panics (in release too) if the size in bytes would overflow 32 bits.
static inline RC_SPAN_ RC_SPAN_MAKE_(uint32_t n, rc_arena *arena)
{
    if (n == 0) {
        return (RC_SPAN_) {0};
    }
    RC_PANIC((uint64_t)n * sizeof(RC_ARRAY_TYPE) <= UINT32_MAX);
    RC_ASSERT(arena);

    RC_ARRAY_TYPE *data = (RC_ARRAY_TYPE *)rc_arena_alloc(arena, (uint32_t)(n * sizeof(RC_ARRAY_TYPE)));
    return (RC_SPAN_) {.data = data, .num = n};
}

// Like make, but the elements are zeroed.
static inline RC_SPAN_ RC_SPAN_MAKE_ZERO_(uint32_t n, rc_arena *arena)
{
    RC_SPAN_ span = RC_SPAN_MAKE_(n, arena);

    if (n > 0) {
        rc_memset(span.data, 0, (uint32_t)(n * sizeof(RC_ARRAY_TYPE)));
    }
    return span;
}

// A span of num elements at data, which it does not own.
static inline RC_SPAN_ RC_SPAN_FROM_PTR_(RC_ARRAY_TYPE *data, uint32_t num)
{
    RC_ASSERT(num == 0 || data != NULL);
    return (RC_SPAN_) {.data = data, .num = num};
}

// The first n elements.
static inline RC_SPAN_ RC_SPAN_FIRST_(RC_SPAN_ span, uint32_t n)
{
    n = n > span.num ? span.num : n;
    return (RC_SPAN_) {.data = span.data, .num = n};
}

// The last n elements.
static inline RC_SPAN_ RC_SPAN_LAST_(RC_SPAN_ span, uint32_t n)
{
    n = n > span.num ? span.num : n;
    return (RC_SPAN_) {.data = span.data + (span.num - n), .num = n};
}

// All but the first n elements.
static inline RC_SPAN_ RC_SPAN_DROP_FIRST_(RC_SPAN_ span, uint32_t n)
{
    n = n > span.num ? span.num : n;
    return (RC_SPAN_) {.data = span.data + n, .num = span.num - n};
}

// All but the last n elements.
static inline RC_SPAN_ RC_SPAN_DROP_LAST_(RC_SPAN_ span, uint32_t n)
{
    n = n > span.num ? span.num : n;
    return (RC_SPAN_) {.data = span.data, .num = span.num - n};
}

// The elements from start up to end, clamped to the span (an end before start gives nothing).
static inline RC_SPAN_ RC_SPAN_SUBSPAN_(RC_SPAN_ span, uint32_t start, uint32_t end)
{
    start = start > span.num ? span.num : start;
    end = end > span.num ? span.num : end;
    end = end < start ? start : end;
    return (RC_SPAN_) {.data = span.data + start, .num = end - start};
}

// Element index by value.
static inline RC_ARRAY_TYPE RC_SPAN_GET_(RC_SPAN_ span, uint32_t index)
{
    RC_ASSERT(RC_SPAN_IS_VALID_(span));
    RC_ASSERT(index < span.num);
    return span.data[index];
}

// Overwrite element index.
static inline void RC_SPAN_SET_(RC_SPAN_ span, uint32_t index, RC_ARRAY_TYPE value)
{
    RC_ASSERT(RC_SPAN_IS_VALID_(span));
    RC_ASSERT(index < span.num);
    span.data[index] = value;
}

// A pointer to element index.
static inline RC_ARRAY_TYPE *RC_SPAN_AT_(RC_SPAN_ span, uint32_t index)
{
    RC_ASSERT(RC_SPAN_IS_VALID_(span));
    RC_ASSERT(index < span.num);
    return &span.data[index];
}

// A pointer to the last element.
static inline RC_ARRAY_TYPE *RC_SPAN_LAST_AT_(RC_SPAN_ span)
{
    RC_ASSERT(RC_SPAN_IS_VALID_(span));
    RC_ASSERT(!RC_SPAN_IS_EMPTY_(span));
    return &span.data[span.num - 1];
}

// Reverse the elements in place.
static inline void RC_SPAN_REVERSE_(RC_SPAN_ span)
{
    uint32_t lo = 0;
    uint32_t hi = span.num;

    while (lo + 1 < hi) {
        RC_ARRAY_TYPE tmp = span.data[lo];

        hi--;
        span.data[lo] = span.data[hi];
        span.data[hi] = tmp;
        lo++;
    }
}

// Swap the first min(x.num, y.num) elements of x and y. Rotate's helper.
static inline void RC_SPAN_SWAP_RANGES_(RC_SPAN_ x, RC_SPAN_ y)
{
    uint32_t count = x.num < y.num ? x.num : y.num;

    for (uint32_t i = 0; i < count; i++) {
        RC_ARRAY_TYPE tmp = x.data[i];

        x.data[i] = y.data[i];
        y.data[i] = tmp;
    }
}

// Rotate left by k, so element k moves to the front. Does nothing unless 0 < k < num (k is not
// taken modulo num).
//
// We use the Gries-Mills block swap: with A = [0, k) and B = [k, num), swap the smaller block into
// its final place and carry on with what is left, Euclid-style. That is num - gcd(num, k) swaps,
// each pass a straight run through memory, in O(1) space.
static inline void RC_SPAN_ROTATE_(RC_SPAN_ span, uint32_t k)
{
    if (k == 0 || k >= span.num) {
        return;
    }
    while (k != 0 && k != span.num) {
        uint32_t b = span.num - k;

        if (k <= b) {
            // A fits in B: swap A with B's first k, which are then in place
            RC_SPAN_SWAP_RANGES_(RC_SPAN_FIRST_(span, k), RC_SPAN_DROP_FIRST_(span, k));
            span = RC_SPAN_DROP_FIRST_(span, k);
        }
        else {
            // B fits in A: swap B with A's last b, which puts B in place at the end
            RC_SPAN_SWAP_RANGES_(RC_SPAN_SUBSPAN_(span, k - b, k), RC_SPAN_DROP_FIRST_(span, k));
            span = RC_SPAN_FIRST_(span, k);
            k -= b;
        }
    }
}

// ---- view ----

// A view of num elements at data, which it does not own.
static inline RC_VIEW_ RC_VIEW_FROM_PTR_(const RC_ARRAY_TYPE *data, uint32_t num)
{
    RC_ASSERT(num == 0 || data != NULL);
    return (RC_VIEW_) {.data = data, .num = num};
}

// The first n elements.
static inline RC_VIEW_ RC_VIEW_FIRST_(RC_VIEW_ view, uint32_t n)
{
    n = n > view.num ? view.num : n;
    return (RC_VIEW_) {.data = view.data, .num = n};
}

// The last n elements.
static inline RC_VIEW_ RC_VIEW_LAST_(RC_VIEW_ view, uint32_t n)
{
    n = n > view.num ? view.num : n;
    return (RC_VIEW_) {.data = view.data + (view.num - n), .num = n};
}

// All but the first n elements.
static inline RC_VIEW_ RC_VIEW_DROP_FIRST_(RC_VIEW_ view, uint32_t n)
{
    n = n > view.num ? view.num : n;
    return (RC_VIEW_) {.data = view.data + n, .num = view.num - n};
}

// All but the last n elements.
static inline RC_VIEW_ RC_VIEW_DROP_LAST_(RC_VIEW_ view, uint32_t n)
{
    n = n > view.num ? view.num : n;
    return (RC_VIEW_) {.data = view.data, .num = view.num - n};
}

// The elements from start up to end, clamped to the view (an end before start gives nothing).
static inline RC_VIEW_ RC_VIEW_SUBVIEW_(RC_VIEW_ view, uint32_t start, uint32_t end)
{
    start = start > view.num ? view.num : start;
    end = end > view.num ? view.num : end;
    end = end < start ? start : end;
    return (RC_VIEW_) {.data = view.data + start, .num = end - start};
}

// Element index by value.
static inline RC_ARRAY_TYPE RC_VIEW_GET_(RC_VIEW_ view, uint32_t index)
{
    RC_ASSERT(RC_VIEW_IS_VALID_(view));
    RC_ASSERT(index < view.num);
    return view.data[index];
}

// A pointer to element index.
static inline const RC_ARRAY_TYPE *RC_VIEW_AT_(RC_VIEW_ view, uint32_t index)
{
    RC_ASSERT(RC_VIEW_IS_VALID_(view));
    RC_ASSERT(index < view.num);
    return &view.data[index];
}

// A pointer to the last element.
static inline const RC_ARRAY_TYPE *RC_VIEW_LAST_AT_(RC_VIEW_ view)
{
    RC_ASSERT(RC_VIEW_IS_VALID_(view));
    RC_ASSERT(!RC_VIEW_IS_EMPTY_(view));
    return &view.data[view.num - 1];
}

// ---- cleanup ----

#undef RC_ARRAY_MAKE_
#undef RC_ARRAY_MAKE_COPY_
#undef RC_ARRAY_FROM_VIEW_
#undef RC_ARRAY_GET_
#undef RC_ARRAY_SET_
#undef RC_ARRAY_AT_
#undef RC_ARRAY_LAST_AT_
#undef RC_ARRAY_IS_VALID_
#undef RC_ARRAY_IS_EMPTY_
#undef RC_ARRAY_RESERVE_
#undef RC_ARRAY_GROW_
#undef RC_ARRAY_RESIZE_
#undef RC_ARRAY_RESIZE_ZERO_
#undef RC_ARRAY_RESET_
#undef RC_ARRAY_RELEASE_
#undef RC_ARRAY_PUSH_
#undef RC_ARRAY_PUSH_N_
#undef RC_ARRAY_PUSH_N_ZERO_
#undef RC_ARRAY_POP_
#undef RC_ARRAY_POP_N_
#undef RC_ARRAY_APPEND_
#undef RC_ARRAY_INSERT_
#undef RC_ARRAY_INSERT_N_
#undef RC_ARRAY_INSERT_N_ZERO_
#undef RC_ARRAY_REMOVE_
#undef RC_ARRAY_REMOVE_N_

#undef RC_SPAN_MAKE_
#undef RC_SPAN_MAKE_ZERO_
#undef RC_SPAN_FROM_PTR_
#undef RC_SPAN_IS_VALID_
#undef RC_SPAN_IS_EMPTY_
#undef RC_SPAN_FIRST_
#undef RC_SPAN_LAST_
#undef RC_SPAN_DROP_FIRST_
#undef RC_SPAN_DROP_LAST_
#undef RC_SPAN_SUBSPAN_
#undef RC_SPAN_GET_
#undef RC_SPAN_SET_
#undef RC_SPAN_AT_
#undef RC_SPAN_LAST_AT_
#undef RC_SPAN_REVERSE_
#undef RC_SPAN_ROTATE_
#undef RC_SPAN_SWAP_RANGES_

#undef RC_VIEW_FROM_PTR_
#undef RC_VIEW_IS_VALID_
#undef RC_VIEW_IS_EMPTY_
#undef RC_VIEW_FIRST_
#undef RC_VIEW_LAST_
#undef RC_VIEW_DROP_FIRST_
#undef RC_VIEW_DROP_LAST_
#undef RC_VIEW_SUBVIEW_
#undef RC_VIEW_GET_
#undef RC_VIEW_AT_
#undef RC_VIEW_LAST_AT_

#endif // RC_ARRAY_DECLARE_ONLY

#undef RC_ARRAY_
#undef RC_SPAN_
#undef RC_VIEW_

#undef RC_ARRAY_NAME
#undef RC_ARRAY_TYPE
#undef RC_ARRAY_DECLARE_ONLY
#undef RC_ARRAY_IMPL_ONLY
