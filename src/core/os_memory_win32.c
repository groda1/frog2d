#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "os_memory.h"

void *OS_MemoryReserve(u64 reserve_size)
{
    return VirtualAlloc(NULL, reserve_size, MEM_RESERVE, PAGE_NOACCESS);
}

void OS_MemoryRelease(void *ptr, u64 size)
{
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
}

bool OS_MemoryCommit(void *ptr, u64 size)
{
    return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}
