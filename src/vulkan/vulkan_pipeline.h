#ifndef VULKAN_PIPELINE_H
#define VULKAN_PIPELINE_H

#include <vulkan/vulkan_core.h>

#include "core.h"
#include "memory_arena.h"

#define MAX_VERTEX_ATTRIBUTES 8



typedef struct _pipeline_config_t pipeline_config_t;
typedef struct _pipeline_t pipeline_t;

struct _pipeline_config_t
{
    const char *vertex_shader_path;
    const char *fragment_shader_path;

    u32 push_constant_size; // 0 = no push constants; vertex stage only for now

    u32 vertex_stride;
    u32 vertex_attribute_count;
    VkVertexInputAttributeDescription vertex_attributes[MAX_VERTEX_ATTRIBUTES];

    bool alpha_blending;

    // TODO uniform buffers / samplers (descriptor set layouts)
    // TODO vertex topology (always triangle list for now)
};

struct _pipeline_t
{
    VkPipeline          vk_pipeline;
    VkPipelineLayout    layout;
    u32                 push_constant_size;

    // TODO descriptor sets
};

bool VulkanPipeline_Create(arena_t *arena, VkDevice device, VkRenderPass render_pass,
                           VkExtent2D extent, const pipeline_config_t *config,
                           pipeline_t *pipeline_out);
void VulkanPipeline_Destroy(VkDevice device, pipeline_t *pipeline);

#endif
