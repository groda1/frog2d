#ifndef VULKAN_PIPELINE_H
#define VULKAN_PIPELINE_H

#include <vulkan/vulkan_core.h>

#include "core.h"

#include "render_types.h"
#include "vulkan_types.h"

typedef struct _pipeline_t pipeline_t;

struct _pipeline_t
{
    pipeline_config_t   config;

    VkPipeline          vk_pipeline;
    VkPipelineLayout    layout;
    u32                 push_constant_size;

    VkDescriptorSetLayout   descriptor_set_layout;
    VkDescriptorPool        descriptor_pool;
    VkDescriptorSet         descriptor_sets[MAX_FRAMES_IN_FLIGHT];
};

bool VulkanPipeline_Create(VkRenderPass render_pass, const pipeline_config_t *config,
                           pipeline_t *pipeline_out);
void VulkanPipeline_Destroy(pipeline_t *pipeline);

#endif
