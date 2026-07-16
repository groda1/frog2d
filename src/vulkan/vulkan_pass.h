#ifndef VULKAN_PASS_H
#define VULKAN_PASS_H

#include <vulkan/vulkan_core.h>

#include "render_types.h"
#include "vulkan_renderer.h"

bool VulkanPass_Init(arena_t *frame_arena);
bool VulkanPass_Destroy();
bool VulkanPass_CreateSwapchainPass(arena_t *arena, swapchain_t *swapchain);
bool VulkanPass_RecreateSwapchainPass(swapchain_t *swapchain);

renderpass_handle_t VulkanPass_CreateImagePass(arena_t *arena, texture_handle_t target_texture,
                                               u32 order);
pipeline_handle_t VulkanPass_AddPipeline(renderpass_handle_t pass_handle,
                                         const pipeline_config_t *config);

void VulkanPass_BeginFrame();
void VulkanPass_AddDrawCommand(const draw_command_t *draw_command);
bool VulkanPass_BakeCommandBuffer(VkCommandBuffer command_buffer, u32 image_index);

#endif
