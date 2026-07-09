#include <vulkan/vulkan_core.h>

#include "core.h"
#include "log.h"

#include "vulkan_context.h"
#include "vulkan_image.h"


static bool create_image(VkExtent2D extent, u32 mip_levels, VkSampleCountFlags num_samples,
                         VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                         VkMemoryPropertyFlags required_memory_properties, VkImage *image_out,
                         VkDeviceMemory *image_memory_out);
static bool find_supported_format(const VkFormat *candidate_formats, u32 candidate_format_count,
                                  VkImageTiling tiling, VkFormatFeatureFlags features,
                                  VkFormat *format_out);
static VkImageLayout host_copy_dst_layout();

bool VulkanImage_CreateView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags,
                            u32 mip_levels, VkImageView *image_view_out)
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

    if (vkCreateImageView(g_device, &create_info, NULL, image_view_out) != VK_SUCCESS)
        return false;

    return true;
}

bool VulkanImage_CreateStatic(u32 width, u32 height, const u8 *rgba_data, VkImage *image_out,
                              VkDeviceMemory *image_memory_out, VkImageLayout *layout_out)
{
    Assert(width > 0 && height > 0 && rgba_data != NULL);
    bool result = false;

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory image_memory = VK_NULL_HANDLE;
    VkImageLayout layout = host_copy_dst_layout();

    if (!create_image((VkExtent2D){width, height}, 1, VK_SAMPLE_COUNT_1_BIT,
                      VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                      VK_IMAGE_USAGE_HOST_TRANSFER_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &image, &image_memory))
        goto exit;

    /* host image copy: layout transition and upload are plain device calls,
       no staging buffer, command buffer or queue submit involved */
    VkHostImageLayoutTransitionInfo transition = {
        .sType = VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO,
        .image = image,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = layout,
        .subresourceRange =
            (VkImageSubresourceRange){
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    };

    if (vkTransitionImageLayout(g_device, 1, &transition) != VK_SUCCESS)
    {
        Log(ERROR, "failed to transition image layout on host");
        goto exit;
    }

    VkMemoryToImageCopy region = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY,
        .pHostPointer = rgba_data,
        .imageSubresource =
            (VkImageSubresourceLayers){
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
        .imageExtent = {
            .width = width,
            .height = height,
            .depth = 1,
        },
    };

    VkCopyMemoryToImageInfo copy_info = {
        .sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO,
        .dstImage = image,
        .dstImageLayout = layout,
        .regionCount = 1,
        .pRegions = &region,
    };

    if (vkCopyMemoryToImage(g_device, &copy_info) != VK_SUCCESS)
    {
        Log(ERROR, "failed to copy memory to image");
        goto exit;
    }

    *image_out = image;
    *image_memory_out = image_memory;
    *layout_out = layout;

    result = true;

exit:
    if (!result)
    {
        if (image != VK_NULL_HANDLE)
            vkDestroyImage(g_device, image, NULL);
        if (image_memory != VK_NULL_HANDLE)
            vkFreeMemory(g_device, image_memory, NULL);
    }

    return result;
}

/* the copy destination layout must be in the device's pCopyDstLayouts;
   SHADER_READ_ONLY_OPTIMAL is preferred (and near-universal), GENERAL is the
   spec-guaranteed fallback */
static VkImageLayout host_copy_dst_layout()
{
    static VkImageLayout s_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (s_layout != VK_IMAGE_LAYOUT_UNDEFINED)
        return s_layout;

    VkImageLayout dst_layouts[32];
    VkPhysicalDeviceHostImageCopyProperties host_copy_properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES,
        .copyDstLayoutCount = ArrayCount(dst_layouts),
        .pCopyDstLayouts = dst_layouts,
    };
    VkPhysicalDeviceProperties2 properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &host_copy_properties,
    };

    vkGetPhysicalDeviceProperties2(g_physical_device, &properties);

    s_layout = VK_IMAGE_LAYOUT_GENERAL;
    for (u32 i = 0; i < host_copy_properties.copyDstLayoutCount; i++)
    {
        if (dst_layouts[i] == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            s_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        }
    }

    return s_layout;
}

bool VulkanImage_CreateDepthResources(VkExtent2D image_extent, VkFormat depth_format,
                                      VkImage *image_out, VkImageView *image_view_out,
                                      VkDeviceMemory *device_memory_out)
{

    if (!create_image(image_extent, 1, VK_SAMPLE_COUNT_1_BIT, depth_format,
                      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image_out, device_memory_out))
        return false;

    if (!VulkanImage_CreateView(*image_out, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, 1,
                                image_view_out))
        return false;

    return true;
}


bool VulkanImage_FindDepthFormat(VkFormat *depth_format_out)
{
    static const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };

    return find_supported_format(candidates, ArrayCount(candidates), VK_IMAGE_TILING_OPTIMAL,
                                 VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                 depth_format_out);
}

static bool create_image(VkExtent2D extent, u32 mip_levels, VkSampleCountFlags num_samples,
                         VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                         VkMemoryPropertyFlags required_memory_properties, VkImage *image_out,
                         VkDeviceMemory *image_memory_out)
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

    if (vkCreateImage(g_device, &image_create_info, NULL /* TODO: Allocator */, image_out) != VK_SUCCESS)
        return false;

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(g_device, *image_out, &memory_requirements);

    // TODO: memory should be allocated by an allocator

    u32 memory_type_index = 0;
    for (u32 i = 0; i < g_memory_properties.memoryTypeCount; i++)
    {
        if ((memory_requirements.memoryTypeBits & (1 << i)) &&
            g_memory_properties.memoryTypes[i].propertyFlags & required_memory_properties)
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

    if (vkAllocateMemory(g_device, &memory_allocate_info, NULL /* TODO: Allocator */, image_memory_out) != VK_SUCCESS)
        return false;
    if (vkBindImageMemory(g_device, *image_out, *image_memory_out, 0) != VK_SUCCESS)
        return false;

    return true;
}

static bool find_supported_format(const VkFormat *candidate_formats, u32 candidate_format_count,
                                  VkImageTiling tiling, VkFormatFeatureFlags features,
                                  VkFormat *format_out)
{
    for (u32 i = 0; i < candidate_format_count; i++)
    {
        VkFormatProperties format_properties;
        vkGetPhysicalDeviceFormatProperties(g_physical_device, candidate_formats[i], &format_properties);

        if ((tiling == VK_IMAGE_TILING_LINEAR &&
                (format_properties.linearTilingFeatures & features) == features) ||
            (tiling == VK_IMAGE_TILING_OPTIMAL &&
                (format_properties.optimalTilingFeatures & features) == features))
        {
            *format_out = candidate_formats[i];
            return true;
        }
    }

    return false;
}
