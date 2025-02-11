#ifndef VULKAN_RENDERER_H
#define VULKAN_RENDERER_H


typedef struct _vk_renderer_t vk_renderer_t;

bool VulkanRenderer_Init(arena_t *arena);
bool VulkanRenderer_Destroy();

#endif

