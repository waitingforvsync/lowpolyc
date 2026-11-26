// test_setjmp.c
//
// The test runner's guarded calls natively, with setjmp and longjmp. The framework is built only
// for tests, so it may use libc where the library cannot.

#include "test_guard.h"

#include <setjmp.h>

// One jump target per level, which is enough as calls at the same level do not nest. A leave at an
// outer level skips the inner call's return, leaving its target stale until the next call there.
static jmp_buf rc_test_jumps[RC_TEST_LEVEL_COUNT];

bool rc_test_guard_call(rc_test_level level, void (*fn)(void *), void *context)
{
    if (setjmp(rc_test_jumps[level])) {
        return false;
    }

    fn(context);
    return true;
}


void rc_test_guard_leave(rc_test_level level)
{
    longjmp(rc_test_jumps[level], 1);
}
