// algorithm/min_element.template.h
//
// Template for finding a view's smallest element: the index of the first element whose key no
// other key orders before, so the leftmost of a tie, or RC_INDEX_NONE if the view is empty.
// Include it again after redefining the control macros for another search.
//
//   RC_MIN_ELEMENT_TYPE      the element type (required)
//   RC_MIN_ELEMENT_NAME      the function's name; defaults to rc_min_element_<type>
//   RC_MIN_ELEMENT_VIEW      the view searched; defaults to rc_view_<type>
//   RC_MIN_ELEMENT_CTX       the type of a context passed to KEY and CMP, by pointer
//   RC_MIN_ELEMENT_KEY       an element's key: (e), or (ctx, e) with a context; defaults to the
//                            element itself
//   RC_MIN_ELEMENT_KEY_TYPE  the key's type, required with KEY
//   RC_MIN_ELEMENT_CMP       true if key a orders before key b: (a, b), or (ctx, a, b) with a
//                            context; defaults to (a) < (b)
//
// The function is uint32_t NAME(view), or (view, ctx) with a context. It projects each element
// once, keeping the smallest key so far.
//
//   #define RC_MIN_ELEMENT_TYPE vertex
//   #define RC_MIN_ELEMENT_KEY(v) ((v).pos.y)
//   #define RC_MIN_ELEMENT_KEY_TYPE float
//   #include "richc/algorithm/min_element.template.h"
//   // uint32_t rc_min_element_vertex(rc_view_vertex view)

// The fallback type keeps IntelliSense happy when it parses this file on its own
#ifndef RC_MIN_ELEMENT_TYPE
#  define RC_MIN_ELEMENT_TYPE int
#  error "RC_MIN_ELEMENT_TYPE must be defined before including richc/algorithm/min_element.template.h"
#endif

#ifndef RC_MIN_ELEMENT_NAME
#  define RC_MIN_ELEMENT_NAME RC_CONCAT(rc_min_element_, RC_MIN_ELEMENT_TYPE)
#endif

#ifndef RC_MIN_ELEMENT_VIEW
#  define RC_MIN_ELEMENT_VIEW RC_CONCAT(rc_view_, RC_MIN_ELEMENT_TYPE)
#endif

#define RC_MIN_ELEMENT_GET_ RC_CONCAT(RC_MIN_ELEMENT_VIEW, _get)
#define RC_MIN_ELEMENT_OF_SOME_ RC_CONCAT(RC_MIN_ELEMENT_NAME, _of_some_)

// ---- what every algorithm shares ----

#define RC_ALGORITHM_TYPE_ RC_MIN_ELEMENT_TYPE

#ifdef RC_MIN_ELEMENT_CTX
#  define RC_ALGORITHM_CTX_ RC_MIN_ELEMENT_CTX
#endif

#ifdef RC_MIN_ELEMENT_KEY
#  define RC_ALGORITHM_KEY_FN_ RC_MIN_ELEMENT_KEY
#endif

#ifdef RC_MIN_ELEMENT_KEY_TYPE
#  define RC_ALGORITHM_KEY_TYPE_ RC_MIN_ELEMENT_KEY_TYPE
#endif

#ifdef RC_MIN_ELEMENT_CMP
#  define RC_ALGORITHM_CMP_FN_ RC_MIN_ELEMENT_CMP
#endif

#include "richc/algorithm/private/begin.template.h"

// ---- the search ----

// The index of the first smallest element of view, which is not empty. Only a strictly smaller key
// moves the answer on, so the leftmost of a tie wins.
static inline uint32_t RC_MIN_ELEMENT_OF_SOME_(RC_MIN_ELEMENT_VIEW view RC_ALGORITHM_CTX_PARAM_)
{
    uint32_t best = 0;
    RC_ALGORITHM_KEY_TYPE_ best_key = RC_ALGORITHM_KEY_(RC_MIN_ELEMENT_GET_(view, 0));

    for (uint32_t i = 1; i < view.num; i++) {
        RC_ALGORITHM_KEY_TYPE_ key = RC_ALGORITHM_KEY_(RC_MIN_ELEMENT_GET_(view, i));

        if (RC_ALGORITHM_LESS_(key, best_key)) {
            best = i;
            best_key = key;
        }
    }
    return best;
}

// The index of the first smallest element of view, or RC_INDEX_NONE if it is empty.
static inline uint32_t RC_MIN_ELEMENT_NAME(RC_MIN_ELEMENT_VIEW view RC_ALGORITHM_CTX_PARAM_)
{
    if (view.num == 0) {
        return RC_INDEX_NONE;
    }
    return RC_MIN_ELEMENT_OF_SOME_(view RC_ALGORITHM_CTX_ARG_);
}

// ---- cleanup ----

#include "richc/algorithm/private/end.template.h"

#undef RC_MIN_ELEMENT_OF_SOME_
#undef RC_MIN_ELEMENT_GET_

#undef RC_MIN_ELEMENT_CMP
#undef RC_MIN_ELEMENT_KEY_TYPE
#undef RC_MIN_ELEMENT_KEY
#undef RC_MIN_ELEMENT_CTX
#undef RC_MIN_ELEMENT_VIEW
#undef RC_MIN_ELEMENT_NAME
#undef RC_MIN_ELEMENT_TYPE
