#include <vulkan/vulkan_core.h>

#include "log.h"

#include "vulkan_buffer.h"
#include "vulkan_context.h"
#include "vulkan_image.h"


static bool create_image(VkExtent2D extent, u32 mip_levels, VkSampleCountFlags num_samples,
                         VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                         VkMemoryPropertyFlags required_memory_properties, VkImage *image_out,
                         VkDeviceMemory *image_memory_out);
static bool find_supported_format(const VkFormat *candidate_formats, u32 candidate_format_count,
                                  VkImageTiling tiling, VkFormatFeatureFlags features,
                                  VkFormat *format_out);
static VkCommandBuffer begin_single_time_command(VkCommandPool command_pool);
static bool end_single_time_command(VkCommandPool command_pool, VkQueue submit_queue,
                                    VkCommandBuffer command_buffer);
static bool transition_image_layout(VkCommandPool command_pool, VkQueue submit_queue,
                                    VkImage image, VkImageLayout old_layout,
                                    VkImageLayout new_layout);
static bool copy_buffer_to_image(VkCommandPool command_pool, VkQueue submit_queue,
                                 VkBuffer buffer, VkImage image, u32 width, u32 height);

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

bool VulkanImage_CreateStatic(VkCommandPool command_pool, VkQueue submit_queue, u32 width,
                              u32 height, const u8 *rgba_data, VkImage *image_out,
                              VkDeviceMemory *image_memory_out)
{
    Assert(width > 0 && height > 0 && rgba_data != NULL);
    bool result = false;
    u64 data_size = (u64)width * (u64)height * 4;

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory image_memory = VK_NULL_HANDLE;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    if (!VulkanBuffer_CreateStaging(rgba_data, data_size, &staging_buffer, &staging_memory))
        return false;

    if (!create_image((VkExtent2D){width, height}, 1, VK_SAMPLE_COUNT_1_BIT,
                      VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &image, &image_memory))
        goto exit;

    if (!transition_image_layout(command_pool, submit_queue, image, VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL))
        goto exit;

    if (!copy_buffer_to_image(command_pool, submit_queue, staging_buffer, image, width, height))
        goto exit;

    if (!transition_image_layout(command_pool, submit_queue, image,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        goto exit;

    *image_out = image;
    *image_memory_out = image_memory;

    result = true;

exit:
    if (!result)
    {
        if (image != VK_NULL_HANDLE)
            vkDestroyImage(g_device, image, NULL);
        if (image_memory != VK_NULL_HANDLE)
            vkFreeMemory(g_device, image_memory, NULL);
    }
    vkDestroyBuffer(g_device, staging_buffer, NULL);
    vkFreeMemory(g_device, staging_memory, NULL);

    return result;
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

static VkCommandBuffer begin_single_time_command(VkCommandPool command_pool)
{
    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer command_buffer;
    if (vkAllocateCommandBuffers(g_device, &allocate_info, &command_buffer) != VK_SUCCESS)
    {
        Log(ERROR, "failed to allocate single time command buffer");
        return VK_NULL_HANDLE;
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS)
    {
        Log(ERROR, "failed to begin single time command buffer");
        vkFreeCommandBuffers(g_device, command_pool, 1, &command_buffer);
        return VK_NULL_HANDLE;
    }

    return command_buffer;
}

/* synchronous submit; waits for the queue to go idle */
static bool end_single_time_command(VkCommandPool command_pool, VkQueue submit_queue,
                                    VkCommandBuffer command_buffer)
{
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };

    bool result = vkEndCommandBuffer(command_buffer) == VK_SUCCESS
        && vkQueueSubmit(submit_queue, 1, &submit_info, VK_NULL_HANDLE) == VK_SUCCESS
        && vkQueueWaitIdle(submit_queue) == VK_SUCCESS;

    if (!result)
        Log(ERROR, "failed to submit single time command buffer");

    vkFreeCommandBuffers(g_device, command_pool, 1, &command_buffer);

    return result;
}

static bool transition_image_layout(VkCommandPool command_pool, VkQueue submit_queue,
                                    VkImage image, VkImageLayout old_layout,
                                    VkImageLayout new_layout)
{
    VkAccessFlags src_access_mask;
    VkAccessFlags dst_access_mask;
    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
        new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        src_access_mask = 0;
        dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dst_access_mask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        src_access_mask = 0;
        dst_access_mask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        Log(ERROR, "unsupported image layout transition %d -> %d", old_layout, new_layout);
        return false;
    }

    VkCommandBuffer command_buffer = begin_single_time_command(command_pool);
    if (command_buffer == VK_NULL_HANDLE)
        return false;

    VkImageMemoryBarrier image_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src_access_mask,
        .dstAccessMask = dst_access_mask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange =
            (VkImageSubresourceRange){
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0, 0, NULL, 0, NULL, 1,
                         &image_barrier);

    return end_single_time_command(command_pool, submit_queue, command_buffer);
}

static bool copy_buffer_to_image(VkCommandPool command_pool, VkQueue submit_queue,
                                 VkBuffer buffer, VkImage image, u32 width, u32 height)
{
    VkCommandBuffer command_buffer = begin_single_time_command(command_pool);
    if (command_buffer == VK_NULL_HANDLE)
        return false;

    VkBufferImageCopy region = {
        .imageSubresource =
            (VkImageSubresourceLayers){
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .imageExtent = {
            .width = width,
            .height = height,
            .depth = 1,
        },
    };

    vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &region);

    return end_single_time_command(command_pool, submit_queue, command_buffer);
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
