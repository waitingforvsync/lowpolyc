// algorithm/private/begin.template.h
//
// What the algorithms over views and spans share: how a context is passed, how an element becomes
// the key that is compared, and how keys are compared. Each algorithm maps its own control macros
// onto the inputs below, includes this, writes its functions with what this defines, and includes
// end.template.h, so they all thread a context and project keys the same way.
//
// The inputs, from the algorithm's TYPE, CTX, KEY, KEY_TYPE and CMP:
//
//   RC_ALGORITHM_TYPE_      the element type
//   RC_ALGORITHM_CTX_       the context type, if there is one
//   RC_ALGORITHM_KEY_FN_    the projection, if there is one: (e), or (ctx, e) with a context
//   RC_ALGORITHM_KEY_TYPE_  the type it projects to; it comes with the projection, or not at all
//   RC_ALGORITHM_CMP_FN_    the comparison of keys, if there is one: (a, b), or (ctx, a, b)
//
// What the algorithm then writes with:
//
//   RC_ALGORITHM_KEY_TYPE_     the key type: the element type when there is no projection
//   RC_ALGORITHM_KEY_(e)       element e's key: e itself when there is no projection
//   RC_ALGORITHM_LESS_(a, b)   true if key a orders before key b; (a) < (b) by default
//   RC_ALGORITHM_CTX_PARAM_    ", CTX *ctx" with a context, else nothing, to follow the container
//                              in a parameter list
//   RC_ALGORITHM_CTX_ARG_      ", ctx" with a context, else nothing, to follow it in a call
//
// A default that has a context to hand ignores it, through (void)ctx, so an unused context gives
// no warning.

// The fallback type keeps IntelliSense happy when it parses this file on its own
#ifndef RC_ALGORITHM_TYPE_
#  define RC_ALGORITHM_TYPE_ int
#  error "richc/algorithm/private/begin.template.h is included by the algorithm templates, not directly"
#endif

#if defined(RC_ALGORITHM_KEY_FN_) && !defined(RC_ALGORITHM_KEY_TYPE_)
#  error "an algorithm's KEY needs its KEY_TYPE, the type it projects to"
#endif

#if defined(RC_ALGORITHM_KEY_TYPE_) && !defined(RC_ALGORITHM_KEY_FN_)
#  error "an algorithm's KEY_TYPE needs its KEY, the projection to it"
#endif

#ifndef RC_ALGORITHM_KEY_TYPE_
#  define RC_ALGORITHM_KEY_TYPE_ RC_ALGORITHM_TYPE_
#endif

#ifdef RC_ALGORITHM_CTX_
#  define RC_ALGORITHM_CTX_PARAM_ , RC_ALGORITHM_CTX_ *ctx
#  define RC_ALGORITHM_CTX_ARG_ , ctx
#  ifdef RC_ALGORITHM_KEY_FN_
#    define RC_ALGORITHM_KEY_(e) RC_ALGORITHM_KEY_FN_(ctx, e)
#  else
#    define RC_ALGORITHM_KEY_(e) ((void)ctx, (e))
#  endif
#  ifdef RC_ALGORITHM_CMP_FN_
#    define RC_ALGORITHM_LESS_(a, b) RC_ALGORITHM_CMP_FN_(ctx, a, b)
#  else
#    define RC_ALGORITHM_LESS_(a, b) ((void)ctx, (a) < (b))
#  endif
#else
#  define RC_ALGORITHM_CTX_PARAM_
#  define RC_ALGORITHM_CTX_ARG_
#  ifdef RC_ALGORITHM_KEY_FN_
#    define RC_ALGORITHM_KEY_(e) RC_ALGORITHM_KEY_FN_(e)
#  else
#    define RC_ALGORITHM_KEY_(e) (e)
#  endif
#  ifdef RC_ALGORITHM_CMP_FN_
#    define RC_ALGORITHM_LESS_(a, b) RC_ALGORITHM_CMP_FN_(a, b)
#  else
#    define RC_ALGORITHM_LESS_(a, b) ((a) < (b))
#  endif
#endif
