#ifndef VULKAN_BUFFER_H
#define VULKAN_BUFFER_H

#include <vulkan/vulkan_core.h>

#include "core.h"

bool VulkanBuffer_Init();
void VulkanBuffer_Destroy(VkDevice device);

bool VulkanBuffer_BakeCommandBuffer(VkDevice device, VkCommandBuffer command_buffer, u32 image_index);

VkBuffer VulkanBuffer_CreateStatic(VkDevice device, VkPhysicalDeviceMemoryProperties memory_prop,
                                   VkCommandPool command_pool, VkQueue submit_queue, const u8 *data,
                                   u64 size, VkBufferUsageFlags usage);

#endif
