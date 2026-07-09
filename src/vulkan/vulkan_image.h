#ifndef VULKAN_IMAGE_H
#define VULKAN_IMAGE_H

#include <vulkan/vulkan.h>

#include "core.h"

VkImageView VulkanImage_CreateView(
    VkDevice device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect_flags,
    u32 mip_levels
);

#endif
