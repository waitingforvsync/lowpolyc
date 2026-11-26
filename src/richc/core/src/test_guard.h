// test_guard.h
//
// How the test runner leaves a test part-way through. A failing check leaves the whole test, and
// rc_test_bail leaves the rc_test_try it is in, so there are two levels of guarded call.
//
// Natively a guarded call is setjmp and a leave is longjmp, in test_setjmp.c. wasm has neither,
// so platform/wasm/test.c has test.js make the call inside a JavaScript try, and a leave throws.

#ifndef RC_TEST_GUARD_H_
#define RC_TEST_GUARD_H_

#include <stdbool.h>

#include "richc/macros.h"

// The levels a guarded call can be left at, outermost first.
typedef enum rc_test_level {
    RC_TEST_LEVEL_TEST,
    RC_TEST_LEVEL_TRY,
    RC_TEST_LEVEL_COUNT
} rc_test_level;

// Call fn(context), returning false if it was left at level. A leave at an outer level passes
// through. Calls at the same level do not nest.
bool rc_test_guard_call(rc_test_level level, void (*fn)(void *), void *context);

// Leave the guarded call at level.
RC_NORETURN void rc_test_guard_leave(rc_test_level level);

#endif // RC_TEST_GUARD_H_
