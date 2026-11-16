// lower_bound.template.h
//
// Template for a binary search on a sorted view: the index of the first element that does not
// order before a key, so the first element >= the key, or the view's num if every element is less.
// Include it again after redefining the control macros for another search.
//
//   RC_LOWER_BOUND_TYPE      the element type (required)
//   RC_LOWER_BOUND_NAME      the name suffix; defaults to RC_LOWER_BOUND_TYPE. The view searched is
//                            rc_view_<name> and the function rc_lower_bound_<name>
//   RC_LOWER_BOUND_KEY_TYPE  the type searched for; defaults to the element type
//   RC_LOWER_BOUND_CMP       true if element a orders before key b: (a, b), or (ctx, a, b) with a
//                            context; defaults to (a) < (b)
//   RC_LOWER_BOUND_CTX       the type of a context passed to the comparison, by pointer
//
// The function is uint32_t rc_lower_bound_<name>(view, key), or (view, ctx, key) with a context.
// The view must be sorted under the comparison, and it may be empty.
//
//   #define RC_LOWER_BOUND_TYPE keyword
//   #define RC_LOWER_BOUND_KEY_TYPE rc_str
//   #define RC_LOWER_BOUND_CMP(a, b) (rc_str_compare((a).word, (b)) < 0)
//   #include "richc/lower_bound.template.h"
//   // uint32_t rc_lower_bound_keyword(rc_view_keyword view, rc_str key)

// The fallback type keeps IntelliSense happy when it parses this file on its own
#ifndef RC_LOWER_BOUND_TYPE
#  define RC_LOWER_BOUND_TYPE int
#  error "RC_LOWER_BOUND_TYPE must be defined before including richc/lower_bound.template.h"
#endif

#ifndef RC_LOWER_BOUND_NAME
#  define RC_LOWER_BOUND_NAME RC_LOWER_BOUND_TYPE
#endif

#ifndef RC_LOWER_BOUND_KEY_TYPE
#  define RC_LOWER_BOUND_KEY_TYPE RC_LOWER_BOUND_TYPE
#endif

#define RC_LOWER_BOUND_VIEW_ RC_CONCAT(rc_view_, RC_LOWER_BOUND_NAME)
#define RC_LOWER_BOUND_GET_ RC_CONCAT(RC_LOWER_BOUND_VIEW_, _get)
#define RC_LOWER_BOUND_FN_ RC_CONCAT(rc_lower_bound_, RC_LOWER_BOUND_NAME)

// The comparison as called, and the parameters that carry the context. The default comparison
// with a context ignores it, through (void)ctx.
#ifdef RC_LOWER_BOUND_CTX
#  ifdef RC_LOWER_BOUND_CMP
#    define RC_LOWER_BOUND_LESS_(a, b) RC_LOWER_BOUND_CMP(ctx, a, b)
#  else
#    define RC_LOWER_BOUND_LESS_(a, b) ((void)ctx, (a) < (b))
#  endif
#  define RC_LOWER_BOUND_PARAMS_ RC_LOWER_BOUND_VIEW_ view, RC_LOWER_BOUND_CTX *ctx
#else
#  ifdef RC_LOWER_BOUND_CMP
#    define RC_LOWER_BOUND_LESS_(a, b) RC_LOWER_BOUND_CMP(a, b)
#  else
#    define RC_LOWER_BOUND_LESS_(a, b) ((a) < (b))
#  endif
#  define RC_LOWER_BOUND_PARAMS_ RC_LOWER_BOUND_VIEW_ view
#endif

// The index of the first element of view that does not order before key, or view.num.
static inline uint32_t RC_LOWER_BOUND_FN_(RC_LOWER_BOUND_PARAMS_, RC_LOWER_BOUND_KEY_TYPE key)
{
    uint32_t lo = 0;
    uint32_t hi = view.num;

    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;

        if (RC_LOWER_BOUND_LESS_(RC_LOWER_BOUND_GET_(view, mid), key)) {
            lo = mid + 1;
        }
        else {
            hi = mid;
        }
    }
    return lo;
}

#undef RC_LOWER_BOUND_PARAMS_
#undef RC_LOWER_BOUND_LESS_
#undef RC_LOWER_BOUND_FN_
#undef RC_LOWER_BOUND_GET_
#undef RC_LOWER_BOUND_VIEW_
#undef RC_LOWER_BOUND_CTX
#undef RC_LOWER_BOUND_CMP
#undef RC_LOWER_BOUND_KEY_TYPE
#undef RC_LOWER_BOUND_NAME
#undef RC_LOWER_BOUND_TYPE
