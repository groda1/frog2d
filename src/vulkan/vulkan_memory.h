
#ifndef VULKAN_MEMORY_H
#define VULKAN_MEMORY_H

#include <vulkan/vulkan_core.h>

#include "core.h"

// TODO proper gpu memory allocator; every buffer/image gets its own
// VkDeviceMemory allocation for now

typedef struct _vulkan_memory_manager_t vulkan_memory_manager_t;
struct _vulkan_memory_manager_t;

bool VulkanMemory_FindMemoryType(VkPhysicalDeviceMemoryProperties memory_properties,
                                 u32 type_bits, VkMemoryPropertyFlags required_properties,
                                 u32 *type_index_out);

#endif
