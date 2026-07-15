#define _POSIX_C_SOURCE 199309L
#include <time.h>

#include "os_time.h"

u64 OS_TimeNowNs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * NS_PER_SECOND + (u64)ts.tv_nsec;
}
