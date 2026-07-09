#ifndef VULKAN_BUFFER_H
#define VULKAN_BUFFER_H

#include <vulkan/vulkan_core.h>

#include "core.h"
#include "memory_arena.h"
#include "render_types.h"

bool VulkanBuffer_Init();
void VulkanBuffer_Destroy();

VkBuffer VulkanBuffer_CreateStatic(VkCommandPool command_pool, VkQueue submit_queue,
                                   const u8 *data, u64 size, VkBufferUsageFlags usage);

/* host-visible transfer source prefilled with data; the caller owns the
   buffer and memory */
bool VulkanBuffer_CreateStaging(const void *data, u64 size, VkBuffer *buffer_out,
                                VkDeviceMemory *memory_out);

buffer_object_handle_t VulkanBuffer_CreateObject(arena_t *arena, u64 capacity,
                                                 buffer_object_type_t type);
bool VulkanBuffer_SetObjectData(buffer_object_handle_t handle, const void *data, u64 size);

VkBuffer VulkanBuffer_GetDeviceBuffer(buffer_object_handle_t handle, u32 frame_index);
u64 VulkanBuffer_GetObjectCapacity(buffer_object_handle_t handle);

bool VulkanBuffer_BakeCommandBuffer(VkCommandBuffer command_buffer, u32 image_index);

#endif
