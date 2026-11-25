// time.c
//
// Windows. The monotonic clock is the performance counter, whose frequency is fixed at boot.

#include "richc/time.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


// The nanoseconds in ticks of a clock running at frequency: the whole seconds, then the rest, so
// no product can overflow.
static uint64_t rc_time_ticks_to_ns(uint64_t ticks, uint64_t frequency)
{
    return ticks / frequency * 1000000000u + ticks % frequency * 1000000000u / frequency;
}


uint64_t rc_time_now_ns(void)
{
    LARGE_INTEGER frequency = {0};
    LARGE_INTEGER count = {0};

    // Both always succeed from Windows XP on
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&count);
    return rc_time_ticks_to_ns((uint64_t)count.QuadPart, (uint64_t)frequency.QuadPart);
}
