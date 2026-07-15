#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "os_time.h"

u64 OS_TimeNowNs(void)
{
    static u64 frequency;
    if (frequency == 0)
    {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        frequency = (u64)freq.QuadPart;
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);

    u64 ticks = (u64)counter.QuadPart;
    return (ticks / frequency) * NS_PER_SECOND
        + (ticks % frequency) * NS_PER_SECOND / frequency;
}
