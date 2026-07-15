#ifndef OS_TIME_H
#define OS_TIME_H

#include "core.h"

#define NS_PER_SECOND 1000000000ull

/* monotonic clock */
u64 OS_TimeNowNs(void);

#endif
