// time.h
//
// A monotonic clock, counting nanoseconds. It never goes backwards and ignores changes to the wall
// clock, but its zero is arbitrary and differs by platform, so only the difference between two
// readings means anything: for timing, and for deadlines.
//
// Natively it reads the system's monotonic clock: CLOCK_MONOTONIC on Linux and macOS, the
// performance counter on Windows. On wasm it reads the page's performance.now(), through
// richc/time.js, which browsers coarsen to tens of microseconds or more.

#ifndef RC_TIME_H_
#define RC_TIME_H_

#include "richc/macros.h"

// The monotonic clock's reading, in nanoseconds.
uint64_t rc_time_now_ns(void);

#endif // RC_TIME_H_
