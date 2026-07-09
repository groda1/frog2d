#ifndef VULKAN_PASS_H
#define VULKAN_PASS_H

#include <vulkan/vulkan_core.h>

#include "render_types.h"
#include "vulkan_renderer.h"

bool VulkanPass_Init(arena_t *frame_arena, VkInstance instance, VkPhysicalDevice physical_device);
bool VulkanPass_Destroy(VkDevice device);
bool VulkanPass_CreateSwapchainPass(
    arena_t *arena, VkDevice device,
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties, swapchain_t *swapchain);
bool VulkanPass_RecreateSwapchainPass(
    VkDevice device, VkPhysicalDeviceMemoryProperties physical_device_memory_properties,
    swapchain_t *swapchain);
pipeline_handle_t VulkanPass_AddPipeline(VkDevice device, renderpass_handle_t pass_handle,
                                         const pipeline_config_t *config);

void VulkanPass_BeginFrame();
void VulkanPass_AddDrawCommand(const draw_command_t *draw_command);
bool VulkanPass_BakeCommandBuffer(VkCommandBuffer command_buffer, u32 image_index);

#endif
