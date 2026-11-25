// algorithm/upper_bound.template.h
//
// Template for a binary search on a sorted view: the index of the first element whose key the key
// searched for orders before, so the first element > it, or the view's num if none is greater.
// With lower_bound, it brackets the run of elements equal to the key. Include it again after
// redefining the control macros for another search.
//
//   RC_UPPER_BOUND_TYPE      the element type (required)
//   RC_UPPER_BOUND_NAME      the function's name; defaults to rc_upper_bound_<type>
//   RC_UPPER_BOUND_VIEW      the view searched; defaults to rc_view_<type>
//   RC_UPPER_BOUND_CTX       the type of a context passed to KEY and CMP, by pointer
//   RC_UPPER_BOUND_KEY       an element's key: (e), or (ctx, e) with a context; defaults to the
//                            element itself
//   RC_UPPER_BOUND_KEY_TYPE  the key's type, required with KEY
//   RC_UPPER_BOUND_CMP       true if key a orders before key b: (a, b), or (ctx, a, b) with a
//                            context; defaults to (a) < (b)
//
// The function is uint32_t NAME(view, key), or (view, ctx, key) with a context, and key is a
// key, not an element. The view must be sorted under the comparison of keys, and it may be empty.
// Each step of the search projects one element.
//
//   #define RC_UPPER_BOUND_TYPE keyword
//   #define RC_UPPER_BOUND_KEY(k) ((k).word)
//   #define RC_UPPER_BOUND_KEY_TYPE rc_str
//   #define RC_UPPER_BOUND_CMP(a, b) (rc_str_compare((a), (b)) < 0)
//   #include "richc/algorithm/upper_bound.template.h"
//   // uint32_t rc_upper_bound_keyword(rc_view_keyword view, rc_str key)

// The fallback type keeps IntelliSense happy when it parses this file on its own
#ifndef RC_UPPER_BOUND_TYPE
#  define RC_UPPER_BOUND_TYPE int
#  error "RC_UPPER_BOUND_TYPE must be defined before including richc/algorithm/upper_bound.template.h"
#endif

#ifndef RC_UPPER_BOUND_NAME
#  define RC_UPPER_BOUND_NAME RC_CONCAT(rc_upper_bound_, RC_UPPER_BOUND_TYPE)
#endif

#ifndef RC_UPPER_BOUND_VIEW
#  define RC_UPPER_BOUND_VIEW RC_CONCAT(rc_view_, RC_UPPER_BOUND_TYPE)
#endif

#define RC_UPPER_BOUND_GET_ RC_CONCAT(RC_UPPER_BOUND_VIEW, _get)

// ---- what every algorithm shares ----

#define RC_ALGORITHM_TYPE_ RC_UPPER_BOUND_TYPE

#ifdef RC_UPPER_BOUND_CTX
#  define RC_ALGORITHM_CTX_ RC_UPPER_BOUND_CTX
#endif

#ifdef RC_UPPER_BOUND_KEY
#  define RC_ALGORITHM_KEY_FN_ RC_UPPER_BOUND_KEY
#endif

#ifdef RC_UPPER_BOUND_KEY_TYPE
#  define RC_ALGORITHM_KEY_TYPE_ RC_UPPER_BOUND_KEY_TYPE
#endif

#ifdef RC_UPPER_BOUND_CMP
#  define RC_ALGORITHM_CMP_FN_ RC_UPPER_BOUND_CMP
#endif

#include "richc/algorithm/private/begin.template.h"

// ---- the search ----

// The index of the first element of view whose key key orders before, or view.num.
static inline uint32_t RC_UPPER_BOUND_NAME(RC_UPPER_BOUND_VIEW view RC_ALGORITHM_CTX_PARAM_, RC_ALGORITHM_KEY_TYPE_ key)
{
    uint32_t lo = 0;
    uint32_t hi = view.num;

    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;

        if (RC_ALGORITHM_LESS_(key, RC_ALGORITHM_KEY_(RC_UPPER_BOUND_GET_(view, mid)))) {
            hi = mid;
        }
        else {
            lo = mid + 1;
        }
    }
    return lo;
}

// ---- cleanup ----

#include "richc/algorithm/private/end.template.h"

#undef RC_UPPER_BOUND_GET_

#undef RC_UPPER_BOUND_CMP
#undef RC_UPPER_BOUND_KEY_TYPE
#undef RC_UPPER_BOUND_KEY
#undef RC_UPPER_BOUND_CTX
#undef RC_UPPER_BOUND_VIEW
#undef RC_UPPER_BOUND_NAME
#undef RC_UPPER_BOUND_TYPE
