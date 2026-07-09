#ifndef VULKAN_RENDERER_H
#define VULKAN_RENDERER_H

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

#include "memory_arena.h"

#include "render_types.h"
#include "vulkan_types.h"

typedef struct _vk_renderer_t vk_renderer_t;


bool VulkanRenderer_Init(arena_t *arena, SDL_Window *window);
bool VulkanRenderer_Destroy();

bool VulkanRenderer_HandleResize(u32 width, u32 height);

void VulkanRenderer_BeginFrame();
bool VulkanRenderer_EndFrame();
void VulkanRenderer_WaitIdle();

VkExtent2D VulkanRenderer_GetExtent();

pipeline_handle_t VulkanRenderer_AddPipeline(renderpass_handle_t pass_handle,
                                             const pipeline_config_t *config);

VkBuffer VulkanRenderer_CreateStaticVertexBuffer(const void *vertices, u64 size);
VkBuffer VulkanRenderer_CreateStaticIndexBuffer(const u32 *indices, u32 index_count);

buffer_object_handle_t VulkanRenderer_CreateUniformBuffer(u64 size, uniform_stage_t stage);
buffer_object_handle_t VulkanRenderer_CreateStorageBuffer(u64 capacity);
bool VulkanRenderer_SetBufferObject(buffer_object_handle_t handle, const void *data, u64 size);

texture_handle_t VulkanRenderer_CreateTexture(u32 width, u32 height, const u8 *rgba_data,
                                              sampler_handle_t sampler);
sampler_handle_t VulkanRenderer_CreateSampler();

#endif
