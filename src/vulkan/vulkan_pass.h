#ifndef VULKAN_PASS_H
#define VULKAN_PASS_H

#include <vulkan/vulkan_core.h>
#include "vulkan_renderer.h"


bool VulkanPass_Init(VkInstance instance, VkPhysicalDevice physical_device);
bool VulkanPass_Destroy(VkDevice device);
bool VulkanPass_CreateSwapchainPass(
    VkDevice device, VkPhysicalDeviceMemoryProperties physical_device_memory_properties,
    swapchain_t *swapchain);
bool VulkanPass_BakeCommandBuffer(VkCommandBuffer command_buffer, u32 image_index);

#endif
