#ifndef VULKAN_RENDERER_H
#define VULKAN_RENDERER_H

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

#include "memory_arena.h"

#include "render_types.h"

#define MAX_FRAMES_IN_FLIGHT 3

typedef struct _vk_renderer_t vk_renderer_t;
typedef struct _swapchain_t swapchain_t;
typedef struct _draw_command_t draw_command_t;

struct _swapchain_t
{
    VkSwapchainKHR handle;

    VkImage     images[MAX_FRAMES_IN_FLIGHT];
    VkImageView image_views[MAX_FRAMES_IN_FLIGHT];
    VkFormat    format;
    VkExtent2D  extent;
};

struct _draw_command_t
{
    renderpass_handle_t pass;
    pipeline_handle_t   pipeline;

    const void *push_constant_data;

    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    u32 index_count;

    // TODO instancing, dynamic buffer draws
};


bool VulkanRenderer_Init(arena_t *arena, SDL_Window *window);
bool VulkanRenderer_Destroy();

void VulkanRenderer_BeginFrame();
bool VulkanRenderer_EndFrame();
void VulkanRenderer_WaitIdle();

VkExtent2D VulkanRenderer_GetExtent();

pipeline_handle_t VulkanRenderer_AddPipeline(renderpass_handle_t pass_handle,
                                             const pipeline_config_t *config);

VkBuffer VulkanRenderer_CreateStaticVertexBuffer(const void *vertices, u64 size);
VkBuffer VulkanRenderer_CreateStaticIndexBuffer(const u32 *indices, u32 index_count);

#endif
