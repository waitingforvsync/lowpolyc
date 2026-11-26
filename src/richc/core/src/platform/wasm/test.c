// test.c
//
// wasm32. The test runner's guarded calls, and the filter it was given. wasm has no setjmp, so
// test.js beside this calls back into the module inside a JavaScript try, and a leave throws.

#include "test_guard.h"

#include <stdint.h>

#include "richc/test.h"

// Call fn(context) inside a try that catches a leave at level, putting the stack pointer back where
// it was. 1 if fn returned.
__attribute__((import_module("richc"), import_name("test_call")))
int32_t rc_wasm_test_call_(uint32_t level, void (*fn)(void *), void *context);

// Throw what leaves the call at level.
__attribute__((import_module("richc"), import_name("test_leave")))
void rc_wasm_test_leave_(uint32_t level);

// Write up to cap bytes of the runner's filter to data, returning how many.
__attribute__((import_module("richc"), import_name("test_filter")))
uint32_t rc_wasm_test_filter_(char *data, uint32_t cap);

// What test_call calls inside its try.
__attribute__((export_name("test_called")))
void rc_wasm_test_called(void (*fn)(void *), void *context);

void rc_wasm_test_called(void (*fn)(void *), void *context)
{
    fn(context);
}


bool rc_test_guard_call(rc_test_level level, void (*fn)(void *), void *context)
{
    return rc_wasm_test_call_((uint32_t)level, fn, context) != 0;
}


void rc_test_guard_leave(rc_test_level level)
{
    rc_wasm_test_leave_((uint32_t)level);
    RC_UNREACHABLE();
}


const char *rc_test_wasm_filter(void)
{
    static char filter[256] = {0};
    uint32_t len = rc_wasm_test_filter_(filter, sizeof filter - 1);

    filter[len] = '\0';
    return filter;
}
