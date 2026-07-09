#include "vulkan_image.h"

#include <vulkan/vulkan_core.h>

static bool create_image(VkDevice device, VkExtent2D extent, u32 mip_levels,
                         VkSampleCountFlags num_samples, VkFormat format, VkImageTiling tiling,
                         VkImageUsageFlags usage, VkMemoryPropertyFlags required_memory_properties,
                         VkPhysicalDeviceMemoryProperties device_memory_properties,
                         VkImage *image_out, VkDeviceMemory *image_memory_out);

bool VulkanImage_CreateView(VkDevice device, VkImage image, VkFormat format,
                                   VkImageAspectFlags aspect_flags, u32 mip_levels,
                                   VkImageView *image_view_out)
{
    VkImageViewCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = (VkComponentMapping){.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                           .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                           .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                           .a = VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange =
            (VkImageSubresourceRange){
                .aspectMask = aspect_flags,
                .baseMipLevel = 0,
                .levelCount = mip_levels,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .image = image,
    };

    if (vkCreateImageView(device, &create_info, NULL, image_view_out) != VK_SUCCESS)
        return false;

    return true;
}

bool VulkanImage_CreateDepthResources(VkDevice device, VkExtent2D image_extent,
                                      VkPhysicalDeviceMemoryProperties device_memory_properties,
                                      VkFormat depth_format, VkImage *image_out,
                                      VkImageView *image_view_out,
                                      VkDeviceMemory *device_memory_out)
{

    if (!create_image(device, image_extent, 1, VK_SAMPLE_COUNT_1_BIT, depth_format,
                      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device_memory_properties, image_out,
                      device_memory_out))
        return false;

    if (VulkanImage_CreateView(device, *image_out, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, 1,
                               image_view_out) != VK_SUCCESS)
        return false;

    return true;
}

static bool create_image(VkDevice device, VkExtent2D extent, u32 mip_levels,
                         VkSampleCountFlags num_samples, VkFormat format, VkImageTiling tiling,
                         VkImageUsageFlags usage, VkMemoryPropertyFlags required_memory_properties,
                         VkPhysicalDeviceMemoryProperties device_memory_properties,
                         VkImage *image_out, VkDeviceMemory *image_memory_out)
{

    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .mipLevels = mip_levels,
        .arrayLayers = 1,
        .samples = num_samples,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .extent = {
            .width = extent.width,
            .height = extent.height,
            .depth = 1
        }
    };

    if (vkCreateImage(device, &image_create_info, NULL /* TODO: Allocator */, image_out) != VK_SUCCESS)
        return false;

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(device, *image_out, &memory_requirements);

    // TODO: memory should be allocated by an allocator

    u32 memory_type_index = 0;
    for (u32 i = 0; i < device_memory_properties.memoryTypeCount; i++)
    {
        if ((memory_requirements.memoryTypeBits & (1 << i)) &&
            device_memory_properties.memoryTypes[i].propertyFlags & required_memory_properties)
        {
            memory_type_index = i;
            break;
        }
    }

    VkMemoryAllocateInfo memory_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = memory_type_index,
    };

    if (vkAllocateMemory(device, &memory_allocate_info, NULL, image_memory_out) != VK_SUCCESS)
        return false;
    if (vkBindImageMemory(device, *image_out, *image_memory_out, 0) != VK_SUCCESS)
        return false;

    return true;
}
