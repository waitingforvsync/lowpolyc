// algorithm/sort.template.h
//
// Template for sorting a span in place, by introsort: quicksort, which falls back to heapsort if
// it goes too deep, so the worst case is O(n log n), and leaves spans of 16 or fewer to insertion
// sort. The sort is not stable.
//
// Include it again after redefining the control macros for another sort.
//
//   RC_SORT_TYPE      the element type (required)
//   RC_SORT_NAME      the function's name; defaults to rc_sort_<type>
//   RC_SORT_SPAN      the span sorted; defaults to rc_span_<type>
//   RC_SORT_CTX       the type of a context passed to KEY and CMP, by pointer
//   RC_SORT_KEY       an element's key: (e), or (ctx, e) with a context; defaults to the element
//                     itself
//   RC_SORT_KEY_TYPE  the key's type, required with KEY
//   RC_SORT_CMP       true if key a orders before key b: (a, b), or (ctx, a, b) with a context;
//                     defaults to (a) < (b)
//   RC_SORT_CACHED    define to project each element only once, into scratch memory
//
// The function is void NAME(span), or (span, ctx) with a context. Afterwards no element's key
// orders before the key of the element ahead of it.
//
// A plain sort projects elements as it compares them, keeping the key of whichever side of a scan
// stays put. A cached sort takes a scratch arena as well, by value, NAME(span, scratch) or
// NAME(span, ctx, scratch): it projects every element once, into num (key, index) pairs, sorts the
// pairs, then puts the elements in their order. That is worth it when a key is dear to work out.
//
//   #define RC_SORT_TYPE face
//   #define RC_SORT_CTX camera
//   #define RC_SORT_KEY(c, f) camera_depth(c, f)
//   #define RC_SORT_KEY_TYPE float
//   #define RC_SORT_CACHED
//   #include "richc/algorithm/sort.template.h"
//   // void rc_sort_face(rc_span_face span, camera *ctx, rc_arena scratch)
//
// How it works: each quicksort pass orders the first, middle and last items, takes the middle one
// as the pivot, and partitions around it. The ordered ends stop the partition's scans, so they need
// no bounds checks. It then sorts the smaller side by recursion and carries on with the larger, so
// the stack never holds more than log2(n) passes. After 2 log2(n) levels, as libstdc++ and libc++
// allow, a span still too big for insertion sort is heapsorted instead. The items are the elements
// themselves, or for a cached sort the pairs.

// The fallback type keeps IntelliSense happy when it parses this file on its own
#ifndef RC_SORT_TYPE
#  define RC_SORT_TYPE int
#  error "RC_SORT_TYPE must be defined before including richc/algorithm/sort.template.h"
#endif

#ifndef RC_SORT_NAME
#  define RC_SORT_NAME RC_CONCAT(rc_sort_, RC_SORT_TYPE)
#endif

#ifndef RC_SORT_SPAN
#  define RC_SORT_SPAN RC_CONCAT(rc_span_, RC_SORT_TYPE)
#endif

#define RC_SORT_GET_ RC_CONCAT(RC_SORT_SPAN, _get)
#define RC_SORT_SET_ RC_CONCAT(RC_SORT_SPAN, _set)

#define RC_SORT_IS_BEFORE_ RC_CONCAT(RC_SORT_NAME, _is_before_)
#define RC_SORT_SWAP_ RC_CONCAT(RC_SORT_NAME, _swap_)
#define RC_SORT_INSERTION_ RC_CONCAT(RC_SORT_NAME, _insertion_)
#define RC_SORT_LARGER_CHILD_ RC_CONCAT(RC_SORT_NAME, _larger_child_)
#define RC_SORT_SIFT_DOWN_ RC_CONCAT(RC_SORT_NAME, _sift_down_)
#define RC_SORT_HEAP_ RC_CONCAT(RC_SORT_NAME, _heap_)
#define RC_SORT_PIVOT_ RC_CONCAT(RC_SORT_NAME, _pivot_)
#define RC_SORT_PARTITION_ RC_CONCAT(RC_SORT_NAME, _partition_)
#define RC_SORT_INTRO_ RC_CONCAT(RC_SORT_NAME, _intro_)
#define RC_SORT_DEPTH_ RC_CONCAT(RC_SORT_NAME, _depth_)
#define RC_SORT_FOLLOW_CYCLE_ RC_CONCAT(RC_SORT_NAME, _follow_cycle_)

// Spans of this many items or fewer are left to insertion sort
#define RC_SORT_SMALL_ 16

// ---- what every algorithm shares ----

#define RC_ALGORITHM_TYPE_ RC_SORT_TYPE

#ifdef RC_SORT_CTX
#  define RC_ALGORITHM_CTX_ RC_SORT_CTX
#endif

#ifdef RC_SORT_KEY
#  define RC_ALGORITHM_KEY_FN_ RC_SORT_KEY
#endif

#ifdef RC_SORT_KEY_TYPE
#  define RC_ALGORITHM_KEY_TYPE_ RC_SORT_KEY_TYPE
#endif

#ifdef RC_SORT_CMP
#  define RC_ALGORITHM_CMP_FN_ RC_SORT_CMP
#endif

#include "richc/algorithm/private/begin.template.h"

// ---- the items sorted ----

// A plain sort sorts the elements, projecting them as it goes. A cached sort sorts pairs of a key
// and the index of the element it came from, with a span of them made by the array template.
#ifdef RC_SORT_CACHED
#  define RC_SORT_ITEM_ RC_CONCAT(RC_SORT_NAME, _item_)
#  define RC_SORT_ITEMS_ RC_CONCAT(rc_span_, RC_SORT_ITEM_)
#  define RC_SORT_ITEM_KEY_(item) ((item).key)

typedef struct RC_SORT_ITEM_ {
    RC_ALGORITHM_KEY_TYPE_ key;
    uint32_t index;
} RC_SORT_ITEM_;

#  define RC_ARRAY_TYPE RC_SORT_ITEM_
#  define RC_ARRAY_NAME RC_SORT_ITEM_
#  include "richc/array.template.h"
#else
#  define RC_SORT_ITEM_ RC_SORT_TYPE
#  define RC_SORT_ITEMS_ RC_SORT_SPAN
#  define RC_SORT_ITEM_KEY_(item) RC_ALGORITHM_KEY_(item)
#endif

#define RC_SORT_ITEMS_MAKE_ RC_CONCAT(RC_SORT_ITEMS_, _make)
#define RC_SORT_ITEMS_FIRST_ RC_CONCAT(RC_SORT_ITEMS_, _first)
#define RC_SORT_ITEMS_DROP_FIRST_ RC_CONCAT(RC_SORT_ITEMS_, _drop_first)
#define RC_SORT_ITEMS_GET_ RC_CONCAT(RC_SORT_ITEMS_, _get)
#define RC_SORT_ITEMS_SET_ RC_CONCAT(RC_SORT_ITEMS_, _set)
#define RC_SORT_ITEMS_AT_ RC_CONCAT(RC_SORT_ITEMS_, _at)

// ---- introsort of the items ----

// True if item i's key orders before item j's.
static inline bool RC_SORT_IS_BEFORE_(RC_SORT_ITEMS_ items RC_ALGORITHM_CTX_PARAM_, uint32_t i, uint32_t j)
{
    return RC_ALGORITHM_LESS_(RC_SORT_ITEM_KEY_(RC_SORT_ITEMS_GET_(items, i)), RC_SORT_ITEM_KEY_(RC_SORT_ITEMS_GET_(items, j)));
}

// Swap items i and j.
static inline void RC_SORT_SWAP_(RC_SORT_ITEMS_ items, uint32_t i, uint32_t j)
{
    RC_SORT_ITEM_ tmp = RC_SORT_ITEMS_GET_(items, i);

    RC_SORT_ITEMS_SET_(items, i, RC_SORT_ITEMS_GET_(items, j));
    RC_SORT_ITEMS_SET_(items, j, tmp);
}

// Insertion sort, for a few items.
static inline void RC_SORT_INSERTION_(RC_SORT_ITEMS_ items RC_ALGORITHM_CTX_PARAM_)
{
    for (uint32_t i = 1; i < items.num; i++) {
        RC_SORT_ITEM_ item = RC_SORT_ITEMS_GET_(items, i);
        RC_ALGORITHM_KEY_TYPE_ key = RC_SORT_ITEM_KEY_(item);
        uint32_t j = i;

        while (j > 0 && RC_ALGORITHM_LESS_(key, RC_SORT_ITEM_KEY_(RC_SORT_ITEMS_GET_(items, j - 1)))) {
            RC_SORT_ITEMS_SET_(items, j, RC_SORT_ITEMS_GET_(items, j - 1));
            j--;
        }
        RC_SORT_ITEMS_SET_(items, j, item);
    }
}

// The child of root in the heap that fills items whose key orders last, or RC_INDEX_NONE if root
// has no children.
static inline uint32_t RC_SORT_LARGER_CHILD_(RC_SORT_ITEMS_ items RC_ALGORITHM_CTX_PARAM_, uint32_t root)
{
    uint32_t left = 2 * root + 1;
    uint32_t right = left + 1;

    if (left >= items.num) {
        return RC_INDEX_NONE;
    }

    if (right >= items.num) {
        return left;
    }

    return RC_SORT_IS_BEFORE_(items RC_ALGORITHM_CTX_ARG_, left, right) ? right : left;
}

// Move the item at root down the max-heap that fills items, lifting larger children into its
// place, until neither child's key orders after its own.
static inline void RC_SORT_SIFT_DOWN_(RC_SORT_ITEMS_ items RC_ALGORITHM_CTX_PARAM_, uint32_t root)
{
    RC_SORT_ITEM_ item = RC_SORT_ITEMS_GET_(items, root);
    RC_ALGORITHM_KEY_TYPE_ key = RC_SORT_ITEM_KEY_(item);
    uint32_t child = RC_SORT_LARGER_CHILD_(items RC_ALGORITHM_CTX_ARG_, root);

    while (child != RC_INDEX_NONE && RC_ALGORITHM_LESS_(key, RC_SORT_ITEM_KEY_(RC_SORT_ITEMS_GET_(items, child)))) {
        RC_SORT_ITEMS_SET_(items, root, RC_SORT_ITEMS_GET_(items, child));
        root = child;
        child = RC_SORT_LARGER_CHILD_(items RC_ALGORITHM_CTX_ARG_, root);
    }
    RC_SORT_ITEMS_SET_(items, root, item);
}

// Heapsort, for when quicksort has gone too deep.
static inline void RC_SORT_HEAP_(RC_SORT_ITEMS_ items RC_ALGORITHM_CTX_PARAM_)
{
    // Make items a max-heap, from its last parent back to the root
    for (uint32_t i = items.num / 2; i > 0; i--) {
        RC_SORT_SIFT_DOWN_(items RC_ALGORITHM_CTX_ARG_, i - 1);
    }

    // Move the largest left to the end of the shrinking heap, then mend the heap
    for (uint32_t end = items.num; end > 1; end--) {
        RC_SORT_SWAP_(items, 0, end - 1);
        RC_SORT_SIFT_DOWN_(RC_SORT_ITEMS_FIRST_(items, end - 1) RC_ALGORITHM_CTX_ARG_, 0);
    }
}

// Order the first, middle and last items, then move the middle one, the pivot, next to the last,
// and return it. Afterwards the first item's key does not order after the pivot's and the last's
// does not order before it. There are more than RC_SORT_SMALL_ items.
static inline RC_SORT_ITEM_ RC_SORT_PIVOT_(RC_SORT_ITEMS_ items RC_ALGORITHM_CTX_PARAM_)
{
    uint32_t mid = items.num / 2;
    uint32_t last = items.num - 1;

    if (RC_SORT_IS_BEFORE_(items RC_ALGORITHM_CTX_ARG_, mid, 0)) {
        RC_SORT_SWAP_(items, 0, mid);
    }

    if (RC_SORT_IS_BEFORE_(items RC_ALGORITHM_CTX_ARG_, last, 0)) {
        RC_SORT_SWAP_(items, 0, last);
    }

    if (RC_SORT_IS_BEFORE_(items RC_ALGORITHM_CTX_ARG_, last, mid)) {
        RC_SORT_SWAP_(items, mid, last);
    }

    RC_SORT_SWAP_(items, mid, last - 1);
    return RC_SORT_ITEMS_GET_(items, last - 1);
}

// Partition items around a median-of-three pivot, and return the pivot's index: no key before it
// orders after the pivot's, and none after it orders before. The scans need no bounds checks, as
// the first item stops the downward one and the pivot the upward one.
static inline uint32_t RC_SORT_PARTITION_(RC_SORT_ITEMS_ items RC_ALGORITHM_CTX_PARAM_)
{
    RC_SORT_ITEM_ pivot_item = RC_SORT_PIVOT_(items RC_ALGORITHM_CTX_ARG_);
    RC_ALGORITHM_KEY_TYPE_ pivot = RC_SORT_ITEM_KEY_(pivot_item);
    uint32_t lo = 1;
    uint32_t hi = items.num - 3;

    while (true) {
        while (RC_ALGORITHM_LESS_(RC_SORT_ITEM_KEY_(RC_SORT_ITEMS_GET_(items, lo)), pivot)) {
            lo++;
        }

        while (RC_ALGORITHM_LESS_(pivot, RC_SORT_ITEM_KEY_(RC_SORT_ITEMS_GET_(items, hi)))) {
            hi--;
        }

        if (lo >= hi) {
            break;
        }

        RC_SORT_SWAP_(items, lo, hi);
        lo++;
        hi--;
    }

    RC_SORT_SWAP_(items, lo, items.num - 2);
    return lo;
}

// Introsort, with depth quicksort levels left before heapsort takes over.
static inline void RC_SORT_INTRO_(RC_SORT_ITEMS_ items RC_ALGORITHM_CTX_PARAM_, uint32_t depth)
{
    while (items.num > RC_SORT_SMALL_ && depth > 0) {
        uint32_t pivot = RC_SORT_PARTITION_(items RC_ALGORITHM_CTX_ARG_);
        RC_SORT_ITEMS_ left = RC_SORT_ITEMS_FIRST_(items, pivot);
        RC_SORT_ITEMS_ right = RC_SORT_ITEMS_DROP_FIRST_(items, pivot + 1);

        // Recurse into the smaller side and carry on with the larger, so the stack stays shallow
        depth--;
        if (left.num < right.num) {
            RC_SORT_INTRO_(left RC_ALGORITHM_CTX_ARG_, depth);
            items = right;
        }
        else {
            RC_SORT_INTRO_(right RC_ALGORITHM_CTX_ARG_, depth);
            items = left;
        }
    }

    if (items.num > RC_SORT_SMALL_) {
        RC_SORT_HEAP_(items RC_ALGORITHM_CTX_ARG_);
    }
    else {
        RC_SORT_INSERTION_(items RC_ALGORITHM_CTX_ARG_);
    }
}

// The quicksort levels allowed for num items: 2 floor(log2(num)).
static inline uint32_t RC_SORT_DEPTH_(uint32_t num)
{
    uint32_t depth = 0;

    for (uint32_t n = num; n > 1; n >>= 1) {
        depth += 2;
    }
    return depth;
}

// ---- the sort ----

#ifdef RC_SORT_CACHED

// Put the cycle of the order in items that passes through start in place: element i of span
// becomes the one items[i] names. Each element moves once, and each item's index is pointed at
// itself as it is done, so a cycle already in place is a fixed point.
static inline void RC_SORT_FOLLOW_CYCLE_(RC_SORT_SPAN span, RC_SORT_ITEMS_ items, uint32_t start)
{
    RC_SORT_TYPE first = RC_SORT_GET_(span, start);
    uint32_t hole = start;
    uint32_t from = RC_SORT_ITEMS_GET_(items, start).index;

    while (from != start) {
        RC_SORT_SET_(span, hole, RC_SORT_GET_(span, from));
        RC_SORT_ITEMS_AT_(items, hole)->index = hole;
        hole = from;
        from = RC_SORT_ITEMS_GET_(items, hole).index;
    }
    RC_SORT_SET_(span, hole, first);
    RC_SORT_ITEMS_AT_(items, hole)->index = hole;
}

// Sort span, projecting each element once into scratch.
static inline void RC_SORT_NAME(RC_SORT_SPAN span RC_ALGORITHM_CTX_PARAM_, rc_arena scratch)
{
    RC_SORT_ITEMS_ items = RC_SORT_ITEMS_MAKE_(span.num, &scratch);

    for (uint32_t i = 0; i < span.num; i++) {
        RC_SORT_ITEMS_SET_(items, i, (RC_SORT_ITEM_) {.key = RC_ALGORITHM_KEY_(RC_SORT_GET_(span, i)), .index = i});
    }

    RC_SORT_INTRO_(items RC_ALGORITHM_CTX_ARG_, RC_SORT_DEPTH_(items.num));
    for (uint32_t i = 0; i < span.num; i++) {
        RC_SORT_FOLLOW_CYCLE_(span, items, i);
    }
}

#else

// Sort span.
static inline void RC_SORT_NAME(RC_SORT_SPAN span RC_ALGORITHM_CTX_PARAM_)
{
    RC_SORT_INTRO_(span RC_ALGORITHM_CTX_ARG_, RC_SORT_DEPTH_(span.num));
}

#endif

// ---- cleanup ----

#include "richc/algorithm/private/end.template.h"

#undef RC_SORT_ITEMS_AT_
#undef RC_SORT_ITEMS_SET_
#undef RC_SORT_ITEMS_GET_
#undef RC_SORT_ITEMS_DROP_FIRST_
#undef RC_SORT_ITEMS_FIRST_
#undef RC_SORT_ITEMS_MAKE_
#undef RC_SORT_ITEM_KEY_
#undef RC_SORT_ITEMS_
#undef RC_SORT_ITEM_

#undef RC_SORT_SMALL_
#undef RC_SORT_FOLLOW_CYCLE_
#undef RC_SORT_DEPTH_
#undef RC_SORT_INTRO_
#undef RC_SORT_PARTITION_
#undef RC_SORT_PIVOT_
#undef RC_SORT_HEAP_
#undef RC_SORT_SIFT_DOWN_
#undef RC_SORT_LARGER_CHILD_
#undef RC_SORT_INSERTION_
#undef RC_SORT_SWAP_
#undef RC_SORT_IS_BEFORE_
#undef RC_SORT_SET_
#undef RC_SORT_GET_

#undef RC_SORT_CACHED
#undef RC_SORT_CMP
#undef RC_SORT_KEY_TYPE
#undef RC_SORT_KEY
#undef RC_SORT_CTX
#undef RC_SORT_SPAN
#undef RC_SORT_NAME
#undef RC_SORT_TYPE
