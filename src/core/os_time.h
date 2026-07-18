#ifndef OS_TIME_H
#define OS_TIME_H

#include "core.h"

#define NS_PER_SECOND 1000000000ull

/* monotonic clock */
u64 OS_TimeNowNs(void);

/* sleeps at least duration_ns, actual resolution is platform dependent */
void OS_SleepNs(u64 duration_ns);

#endif
