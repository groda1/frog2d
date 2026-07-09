#ifndef VULKAN_PIPELINE_H
#define VULKAN_PIPELINE_H

#include <vulkan/vulkan_core.h>

#include "core.h"
#include "memory_arena.h"

#include "render_types.h"

typedef struct _pipeline_t pipeline_t;

struct _pipeline_t
{
    VkPipeline          vk_pipeline;
    VkPipelineLayout    layout;
    u32                 push_constant_size;

    pipeline_config_t   config;

    // TODO descriptor sets
};

bool VulkanPipeline_Create(arena_t *arena, VkDevice device, VkRenderPass render_pass,
                           VkExtent2D extent, const pipeline_config_t *config,
                           pipeline_t *pipeline_out);
void VulkanPipeline_Destroy(VkDevice device, pipeline_t *pipeline);

#endif
