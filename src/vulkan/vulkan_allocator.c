
#include <stdlib.h>

#include "log.h"
#include <vulkan/vulkan_core.h>


static const char *scope_to_str(VkSystemAllocationScope scope);

static void *vkAllocationFn(
    void *pUserData,
    size_t size,
    size_t alignment,
    VkSystemAllocationScope allocationScope)
{
    Log(DEBUG, "vkAllocationFn size=%d scope=%s", size, scope_to_str(allocationScope));

    return malloc(size);
}

static void *vkReallocationFn(
    void *pUserData,
    void *pOriginal,
    size_t size,
    size_t alignment,
    VkSystemAllocationScope allocationScope)
{
    Log(DEBUG, "vkReallocationFn size=%d scope=%s", size, scope_to_str(allocationScope));
    return realloc(pOriginal, size);
}
static void vkFreeFn(
    void *pUserData,
    void *pMemory)
{
    Log(DEBUG, "vkFreeFn %p", pMemory);
    free(pMemory);
}

static void vkInternalAllocationNotificationFn(
    void*                                       pUserData,
    size_t                                      size,
    VkInternalAllocationType                    allocationType,
    VkSystemAllocationScope                     allocationScope)
{
    Log(DEBUG, "vkInternalAllocationNotificationFn");
}

static void vkInternalFreeNotificationFn(
    void*                                       pUserData,
    size_t                                      size,
    VkInternalAllocationType                    allocationType,
    VkSystemAllocationScope                     allocationScope)
{
    Log(DEBUG, "vkInternalFreeNotificationFn");
}

static VkAllocationCallbacks allocator = {
    .pfnAllocation = vkAllocationFn,
    .pfnReallocation = vkReallocationFn,
    .pfnFree = vkFreeFn,
    .pfnInternalAllocation = vkInternalAllocationNotificationFn,
    .pfnInternalFree = vkInternalFreeNotificationFn
};

bool VulkanAllocator_Init()
{
    return true;
}
VkAllocationCallbacks *VulkanAllocator_Get()
{
    return &allocator;
}

static const char *scope_to_str(VkSystemAllocationScope scope)
{
    switch (scope)
    {
    case VK_SYSTEM_ALLOCATION_SCOPE_COMMAND:
        return "command";
    case VK_SYSTEM_ALLOCATION_SCOPE_OBJECT:
        return "object";
    case VK_SYSTEM_ALLOCATION_SCOPE_CACHE:
        return "cache";
    case VK_SYSTEM_ALLOCATION_SCOPE_DEVICE:
        return "device";
    case VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE:
        return "instance";
    }
}
