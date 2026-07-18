#define _POSIX_C_SOURCE 199309L
#include <errno.h>
#include <time.h>

#include "os_time.h"

u64 OS_TimeNowNs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * NS_PER_SECOND + (u64)ts.tv_nsec;
}

void OS_SleepNs(u64 duration_ns)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(duration_ns / NS_PER_SECOND);
    ts.tv_nsec = (long)(duration_ns % NS_PER_SECOND);

    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
        ;
}
