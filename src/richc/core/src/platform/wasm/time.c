// time.c
//
// wasm32. The monotonic clock is the page's performance.now(), in milliseconds since the page
// loaded, which time.js beside this supplies.

#include "richc/time.h"

// Milliseconds on the page's monotonic clock, with a fraction.
__attribute__((import_module("richc"), import_name("time_now_ms")))
double rc_wasm_time_now_ms_(void);

uint64_t rc_time_now_ns(void)
{
    return (uint64_t)(rc_wasm_time_now_ms_() * 1000000.0);
}
