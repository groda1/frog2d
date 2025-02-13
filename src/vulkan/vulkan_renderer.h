#ifndef VULKAN_RENDERER_H
#define VULKAN_RENDERER_H

#include <SDL3/SDL_vulkan.h>

#include "core.h"


typedef struct _vk_renderer_t vk_renderer_t;

bool VulkanRenderer_Init(arena_t *arena, SDL_Window *window);
bool VulkanRenderer_Destroy();

#endif

