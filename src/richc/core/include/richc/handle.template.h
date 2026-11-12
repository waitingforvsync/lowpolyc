// handle.template.h
//
// Template for a handle type: a genpool slot's index, and that slot's generation when the handle
// was issued. The genpool compares generations to catch a handle that has outlived its element.
//
//   RC_HANDLE_TYPE  the name of the handle type (required)
//
// Generating the handle type apart from its genpool lets a public header declare it while the pool
// that issues it stays private: the genpool takes the name through RC_GENPOOL_HANDLE.
//
// {0} is none. No slot validates it, since a genpool only issues odd generations.

// ---- shared by every instantiation ----

#ifndef RC_HANDLE_TEMPLATE_H_
#define RC_HANDLE_TEMPLATE_H_

#include "richc/macros.h"

#endif // RC_HANDLE_TEMPLATE_H_

// ---- per-type ----

// The fallback name keeps IntelliSense happy when it parses this file on its own
#ifndef RC_HANDLE_TYPE
#  define RC_HANDLE_TYPE rc_handle_int
#  error "RC_HANDLE_TYPE must be defined before including richc/handle.template.h"
#endif

#define RC_HANDLE_IS_NONE_ RC_CONCAT(RC_HANDLE_TYPE, _is_none)
#define RC_HANDLE_IS_EQUAL_ RC_CONCAT(RC_HANDLE_TYPE, _is_equal)

typedef struct RC_HANDLE_TYPE {
    uint32_t index_;  // the slot's index
    uint32_t gen_;    // the slot's generation when the handle was issued; odd, or 0 for none
} RC_HANDLE_TYPE;

// True for the none handle. Any other handle may still be stale: only its pool can tell.
static inline bool RC_HANDLE_IS_NONE_(RC_HANDLE_TYPE handle)
{
    return handle.gen_ == 0;
}

// True when a and b name the same slot in the same generation.
static inline bool RC_HANDLE_IS_EQUAL_(RC_HANDLE_TYPE a, RC_HANDLE_TYPE b)
{
    return a.index_ == b.index_ && a.gen_ == b.gen_;
}

#undef RC_HANDLE_IS_NONE_
#undef RC_HANDLE_IS_EQUAL_

#undef RC_HANDLE_TYPE
