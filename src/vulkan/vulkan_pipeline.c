#include "core.h"
#include "log.h"

#include "render_types.h"
#include "vulkan_buffer.h"
#include "vulkan_context.h"
#include "vulkan_pipeline.h"
#include "vulkan_texture.h"

static VkFormat vertex_format_to_vk(vertex_format_t format);
static VkShaderStageFlags uniform_stage_to_vk(uniform_stage_t stage);
static bool create_shader_module(shader_code_t shader, VkShaderModule *module_out);
static bool create_descriptor_sets(const pipeline_config_t *config, pipeline_t *pipeline);

bool VulkanPipeline_Create(VkFormat color_format, VkFormat depth_format,
                           const pipeline_config_t *config, pipeline_t *pipeline_out)
{
    Assert(config->vertex_attribute_count <= MAX_VERTEX_ATTRIBUTES);

    bool result = false;

    MemoryZeroItem(pipeline_out);

    VkShaderModule vertex_shader = VK_NULL_HANDLE;
    VkShaderModule fragment_shader = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;

    if (!create_shader_module(config->vertex_shader, &vertex_shader) ||
        !create_shader_module(config->fragment_shader, &fragment_shader))
        goto exit;

    if (!create_descriptor_sets(config, pipeline_out))
        goto exit;

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_shader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragment_shader,
            .pName = "main",
        },
    };

    VkVertexInputBindingDescription vertex_binding = {
        .binding = 0,
        .stride = config->vertex_stride,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription vertex_attributes[MAX_VERTEX_ATTRIBUTES];
    for (u32 i = 0; i < config->vertex_attribute_count; i++)
    {
        vertex_attributes[i] = (VkVertexInputAttributeDescription){
            .location = config->vertex_attributes[i].location,
            .binding = 0,
            .format = vertex_format_to_vk(config->vertex_attributes[i].format),
            .offset = config->vertex_attributes[i].offset,
        };
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_binding,
        .vertexAttributeDescriptionCount = config->vertex_attribute_count,
        .pVertexAttributeDescriptions = vertex_attributes,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = false,
    };

    /* viewport and scissor are dynamic (set at bake time) so pipelines are
       independent of the swapchain extent and survive recreation */
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ArrayCount(dynamic_states),
        .pDynamicStates = dynamic_states,
    };

    VkPipelineRasterizationStateCreateInfo rasterization_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisample_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = config->disable_depth_test ? false : true,
        .depthWriteEnable = config->disable_depth_test ? false : true,
        .depthCompareOp = VK_COMPARE_OP_LESS,
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    if (config->alpha_blending)
    {
        color_blend_attachment.blendEnable = true;
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo color_blend_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = config->push_constant_size,
    };

    /* set 0 is always the global bindless texture array so it stays
       compatible across all pipelines; per-pipeline uniforms live in set 1 */
    VkDescriptorSetLayout set_layouts[] = {
        VulkanTexture_GetDescriptorSetLayout(),
        pipeline_out->descriptor_set_layout,
    };

    VkPipelineLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = config->push_constant_size > 0 ? 1U : 0U,
        .pPushConstantRanges = &push_constant_range,
        .setLayoutCount = pipeline_out->descriptor_set_layout != VK_NULL_HANDLE ? 2U : 1U,
        .pSetLayouts = set_layouts,
    };

    if (vkCreatePipelineLayout(g_device, &layout_create_info, NULL, &layout) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create pipeline layout");
        goto exit;
    }

    /* dynamic rendering: the pipeline binds to attachment formats, not a
       render pass object */
    VkPipelineRenderingCreateInfo rendering_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_format,
        .depthAttachmentFormat = depth_format,
    };

    VkGraphicsPipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_create_info,
        .stageCount = ArrayCount(shader_stages),
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pViewportState = &viewport_state,
        .pDynamicState = &dynamic_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &depth_stencil_state,
        .pColorBlendState = &color_blend_state,
        .layout = layout,
    };

    VkPipeline vk_pipeline;
    if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipeline_create_info, NULL,
                                  &vk_pipeline) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create graphics pipeline");
        goto exit;
    }

    pipeline_out->vk_pipeline = vk_pipeline;
    pipeline_out->layout = layout;
    pipeline_out->push_constant_size = config->push_constant_size;
    MemoryCopyStruct(&pipeline_out->config, config);
    layout = VK_NULL_HANDLE;

    result = true;
    Log(INFO, "Created pipeline: %s", config->name);

exit:
    if (!result)
    {
        if (pipeline_out->descriptor_pool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(g_device, pipeline_out->descriptor_pool, NULL);
        if (pipeline_out->descriptor_set_layout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(g_device, pipeline_out->descriptor_set_layout, NULL);
    }
    if (layout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(g_device, layout, NULL);
    if (vertex_shader != VK_NULL_HANDLE)
        vkDestroyShaderModule(g_device, vertex_shader, NULL);
    if (fragment_shader != VK_NULL_HANDLE)
        vkDestroyShaderModule(g_device, fragment_shader, NULL);

    return result;
}

void VulkanPipeline_Destroy(pipeline_t *pipeline)
{
    if (pipeline->descriptor_pool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(g_device, pipeline->descriptor_pool, NULL);
    if (pipeline->descriptor_set_layout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(g_device, pipeline->descriptor_set_layout, NULL);

    vkDestroyPipeline(g_device, pipeline->vk_pipeline, NULL);
    vkDestroyPipelineLayout(g_device, pipeline->layout, NULL);

    MemoryZeroItem(pipeline);
}

static bool create_descriptor_sets(const pipeline_config_t *config, pipeline_t *pipeline)
{
    if (config->uniform_binding_count == 0)
        return true;

    Assert(config->uniform_binding_count <= MAX_UNIFORM_BINDINGS);

    VkDescriptorSetLayoutBinding layout_bindings[MAX_UNIFORM_BINDINGS];
    for (u32 i = 0; i < config->uniform_binding_count; i++)
    {
        const uniform_binding_t *binding = &config->uniform_bindings[i];
        layout_bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding = binding->binding,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = uniform_stage_to_vk(binding->stage),
        };
    }

    VkDescriptorSetLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = config->uniform_binding_count,
        .pBindings = layout_bindings,
    };

    if (vkCreateDescriptorSetLayout(g_device, &layout_create_info, NULL,
                                    &pipeline->descriptor_set_layout) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create descriptor set layout");
        return false;
    }

    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = config->uniform_binding_count * MAX_FRAMES_IN_FLIGHT,
    };

    // TODO a shared descriptor pool instead of one per pipeline
    VkDescriptorPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };

    if (vkCreateDescriptorPool(g_device, &pool_create_info, NULL,
                               &pipeline->descriptor_pool) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create descriptor pool");
        return false;
    }

    VkDescriptorSetLayout set_layouts[MAX_FRAMES_IN_FLIGHT];
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        set_layouts[i] = pipeline->descriptor_set_layout;

    VkDescriptorSetAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pipeline->descriptor_pool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = set_layouts,
    };

    if (vkAllocateDescriptorSets(g_device, &allocate_info, pipeline->descriptor_sets) != VK_SUCCESS)
    {
        Log(ERROR, "failed to allocate descriptor sets");
        return false;
    }

    for (u32 frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++)
    {
        VkDescriptorBufferInfo buffer_infos[MAX_UNIFORM_BINDINGS];
        VkWriteDescriptorSet writes[MAX_UNIFORM_BINDINGS];

        for (u32 i = 0; i < config->uniform_binding_count; i++)
        {
            const uniform_binding_t *binding = &config->uniform_bindings[i];

            buffer_infos[i] = (VkDescriptorBufferInfo){
                .buffer = VulkanBuffer_GetDeviceBuffer(binding->buffer_object, frame),
                .offset = 0,
                .range = VulkanBuffer_GetObjectCapacity(binding->buffer_object),
            };

            writes[i] = (VkWriteDescriptorSet){
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = pipeline->descriptor_sets[frame],
                .dstBinding = binding->binding,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &buffer_infos[i],
            };
        }

        vkUpdateDescriptorSets(g_device, config->uniform_binding_count, writes, 0, NULL);
    }

    return true;
}

static VkShaderStageFlags uniform_stage_to_vk(uniform_stage_t stage)
{
    switch (stage)
    {
    case UNIFORM_STAGE_VERTEX:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case UNIFORM_STAGE_FRAGMENT:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    return 0;
}

static VkFormat vertex_format_to_vk(vertex_format_t format)
{
    switch (format)
    {
    case VERTEX_FORMAT_F32:
        return VK_FORMAT_R32_SFLOAT;
    case VERTEX_FORMAT_F32X2:
        return VK_FORMAT_R32G32_SFLOAT;
    case VERTEX_FORMAT_F32X3:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case VERTEX_FORMAT_F32X4:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }

    return VK_FORMAT_UNDEFINED;
}

static bool create_shader_module(shader_code_t shader, VkShaderModule *module_out)
{
    if (shader.code == NULL || shader.size == 0)
    {
        Log(ERROR, "invalid shader code");
        return false;
    }

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shader.size,
        .pCode = (const u32 *)shader.code,
    };

    if (vkCreateShaderModule(g_device, &create_info, NULL, module_out) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create shader module");
        return false;
    }

    return true;
}
