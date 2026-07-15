#include <sys/mman.h>

#include "os_memory.h"

void *OS_MemoryReserve(u64 reserve_size)
{
    void *ptr = mmap(0, reserve_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
        return NULL;
    return ptr;
}

void OS_MemoryRelease(void *ptr, u64 size)
{
    munmap(ptr, size);
}

bool OS_MemoryCommit(void *ptr, u64 size)
{
    return mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0;
}

