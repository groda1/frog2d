#ifndef VULKAN_BUFFER_H
#define VULKAN_BUFFER_H

#include <vulkan/vulkan_core.h>

bool VulkanBuffer_Create(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties,
                         VkDeviceSize size, VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags memory_flags, VkBuffer *buffer_out,
                         VkDeviceMemory *memory_out);

#endif
