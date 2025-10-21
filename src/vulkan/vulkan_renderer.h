#ifndef VULKAN_RENDERER_H
#define VULKAN_RENDERER_H

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

#include "memory_arena.h"


#define MAX_FRAMES_IN_FLIGHT 3

typedef struct _vk_renderer_t vk_renderer_t;
typedef struct _swapchain_t swapchain_t;

struct _swapchain_t
{
    VkSwapchainKHR handle;

    VkImage images[MAX_FRAMES_IN_FLIGHT];
    VkImageView image_views[MAX_FRAMES_IN_FLIGHT];
    VkFormat format;
    VkExtent2D extent;
};


bool VulkanRenderer_Init(arena_t *arena, SDL_Window *window);
bool VulkanRenderer_Destroy();

#endif
