#ifndef VULKAN_IMAGE_H
#define VULKAN_IMAGE_H

#include <vulkan/vulkan_core.h>

#include "core.h"

bool VulkanImage_CreateView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags,
                            u32 mip_levels, VkImageView *image_view_out);

/* device-local VK_FORMAT_R8G8B8A8_SRGB sampled image uploaded via host image
   copy from rgba_data (width * height * 4 bytes); layout_out is the layout
   the image is left in, for descriptor writes */
bool VulkanImage_CreateStatic(u32 width, u32 height, const u8 *rgba_data, VkImage *image_out,
                              VkDeviceMemory *image_memory_out, VkImageLayout *layout_out);

bool VulkanImage_CreateDepthResources(VkExtent2D image_extent, VkFormat depth_format,
                                      VkImage *image_out, VkImageView *image_view_out,
                                      VkDeviceMemory *device_memory_out);

bool VulkanImage_FindDepthFormat(VkFormat *depth_format_out);

#endif
