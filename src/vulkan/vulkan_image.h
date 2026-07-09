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

bool VulkanImage_FindDepthFormat(VkInstance instance, VkPhysicalDevice physical_device,
                                 VkFormat *depth_format_out);

bool VulkanImage_CreateFramebuffer(VkDevice device, VkImageView image_view,
                                   VkImageView depth_image_view, VkExtent2D extent,
                                   VkRenderPass render_pass, VkFramebuffer *framebuffer_out);

bool VulkanImage_CreateFramebuffers(VkDevice device, const VkImageView *color_image_views,
                                    u32 color_image_view_count, VkImageView depth_image_view,
                                    VkExtent2D extent, VkRenderPass render_pass,
                                    VkFramebuffer *framebuffers_out);

#endif
