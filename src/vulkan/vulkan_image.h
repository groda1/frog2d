#ifndef VULKAN_IMAGE_H
#define VULKAN_IMAGE_H

#include <vulkan/vulkan_core.h>

#include "core.h"

bool VulkanImage_CreateView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags,
                            u32 mip_levels, VkImageView *image_view_out);

/* device-local VK_FORMAT_R8G8B8A8_SRGB sampled image uploaded synchronously
   from rgba_data (width * height * 4 bytes) and transitioned to
   SHADER_READ_ONLY_OPTIMAL */
bool VulkanImage_CreateStatic(VkCommandPool command_pool, VkQueue submit_queue, u32 width,
                              u32 height, const u8 *rgba_data, VkImage *image_out,
                              VkDeviceMemory *image_memory_out);

bool VulkanImage_CreateDepthResources(VkExtent2D image_extent, VkFormat depth_format,
                                      VkImage *image_out, VkImageView *image_view_out,
                                      VkDeviceMemory *device_memory_out);

bool VulkanImage_FindDepthFormat(VkFormat *depth_format_out);

#endif
