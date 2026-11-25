// time.c
//
// Linux and macOS. The monotonic clock is CLOCK_MONOTONIC.

// clock_gettime is POSIX 1993
#define _POSIX_C_SOURCE 199309L

#include "richc/time.h"

#include <time.h>


uint64_t rc_time_now_ns(void)
{
    struct timespec now = {0};

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000u + (uint64_t)now.tv_nsec;
}
