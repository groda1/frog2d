#ifndef OS_MEMORY_H
#define OS_MEMORY_H

#include "core.h"

void *OS_MemoryReserve(u64 reserve_size);
void OS_MemoryRelease(void *ptr, u64 size);
bool OS_MemoryCommit(void *ptr, u64 size);

#endif
