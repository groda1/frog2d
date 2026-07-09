#include <vulkan/vulkan_core.h>

#include "vulkan_context.h"
#include "vulkan_image.h"


static bool create_image(VkExtent2D extent, u32 mip_levels, VkSampleCountFlags num_samples,
                         VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                         VkMemoryPropertyFlags required_memory_properties, VkImage *image_out,
                         VkDeviceMemory *image_memory_out);
static bool find_supported_format(const VkFormat *candidate_formats, u32 candidate_format_count,
                                  VkImageTiling tiling, VkFormatFeatureFlags features,
                                  VkFormat *format_out);

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

bool VulkanImage_CreateFramebuffer(VkImageView image_view, VkImageView depth_image_view,
                                   VkExtent2D extent, VkRenderPass render_pass,
                                   VkFramebuffer *framebuffer_out)
{
    VkImageView attachments[] = {image_view, depth_image_view};

    VkFramebufferCreateInfo framebuffer_create_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass,
        .attachmentCount = ArrayCount(attachments),
        .pAttachments = attachments,
        .width = extent.width,
        .height = extent.height,
        .layers = 1,
    };

    if (vkCreateFramebuffer(g_device, &framebuffer_create_info, NULL, framebuffer_out) != VK_SUCCESS)
        return false;

    return true;
}

bool VulkanImage_CreateFramebuffers(const VkImageView *color_image_views,
                                    u32 color_image_view_count, VkImageView depth_image_view,
                                    VkExtent2D extent, VkRenderPass render_pass,
                                    VkFramebuffer *framebuffers_out)
{
    for (u32 i = 0; i < color_image_view_count; i++)
    {
        if (!VulkanImage_CreateFramebuffer(color_image_views[i], depth_image_view, extent,
                                           render_pass, &framebuffers_out[i]))
            return false;
    }

    return true;
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
