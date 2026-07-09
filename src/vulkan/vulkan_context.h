#ifndef VULKAN_CONTEXT_H
#define VULKAN_CONTEXT_H

#include <vulkan/vulkan_core.h>

/* backend-global vulkan state; owned and initialized by vulkan_renderer.
   valid between VulkanRenderer_Init and VulkanRenderer_Destroy */

extern VkDevice                         g_device;
extern VkPhysicalDevice                 g_physical_device;
extern VkPhysicalDeviceMemoryProperties g_memory_properties;

#endif
