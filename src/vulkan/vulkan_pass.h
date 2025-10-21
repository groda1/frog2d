#ifndef VULKAN_PASS_H
#define VULKAN_PASS_H

#include <vulkan/vulkan_core.h>
#include "vulkan_renderer.h"


void VulkanPass_CreateSwapchainPass(
    VkDevice device, VkPhysicalDeviceMemoryProperties physical_device_memory_properties,
    swapchain_t *swapchain);

#endif
