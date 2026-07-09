#ifndef VULKAN_RENDERER_H
#define VULKAN_RENDERER_H


typedef struct 
{
    arena_t *arena;

    /* data */
} vk_renderer_t;


bool VulkanRenderer_Init(arena_t *arena);


#endif

