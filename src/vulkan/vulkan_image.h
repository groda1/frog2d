#ifndef VULKAN_IMAGE_H
#define VULKAN_IMAGE_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "core.h"

bool VulkanImage_CreateView(VkDevice device, VkImage image, VkFormat format,
                            VkImageAspectFlags aspect_flags, u32 mip_levels,
                            VkImageView *image_view_out);

bool VulkanImage_CreateDepthResources(VkDevice device, VkExtent2D image_extent,
                                      VkPhysicalDeviceMemoryProperties device_memory_properties,
                                      VkFormat depth_format, VkImage *image_out,
                                      VkImageView *image_view_out,
                                      VkDeviceMemory *device_memory_out);

#endif
