#include <stdio.h>

#include <SDL3/SDL_filesystem.h>

#include "core.h"
#include "log.h"

#include "vulkan_pipeline.h"

#define MAX_SHADER_PATH 512

static VkFormat vertex_format_to_vk(vertex_format_t format);
static u8 *read_shader_file(arena_t *arena, const char *path, u64 *size_out);
static bool create_shader_module(VkDevice device, arena_t *scratch, const char *path,
                                 VkShaderModule *module_out);

bool VulkanPipeline_Create(arena_t *arena, VkDevice device, VkRenderPass render_pass,
                           VkExtent2D extent, const pipeline_config_t *config,
                           pipeline_t *pipeline_out)
{
    Assert(config->vertex_attribute_count <= MAX_VERTEX_ATTRIBUTES);

    bool result = false;
    u64 scratch_pos = MemoryArena_Pos(arena);

    VkShaderModule vertex_shader = VK_NULL_HANDLE;
    VkShaderModule fragment_shader = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;

    if (!create_shader_module(device, arena, config->vertex_shader_path, &vertex_shader) ||
        !create_shader_module(device, arena, config->fragment_shader_path, &fragment_shader))
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

    VkViewport viewport = {
        .x = 0.0f,
        .y = (f32)extent.height,
        .width = (f32)extent.width,
        .height = -(f32)extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = extent,
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
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
        .depthTestEnable = true,
        .depthWriteEnable = true,
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
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = config->push_constant_size,
    };

    VkPipelineLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = config->push_constant_size > 0 ? 1U : 0U,
        .pPushConstantRanges = &push_constant_range,
        // TODO descriptor set layouts
    };

    if (vkCreatePipelineLayout(device, &layout_create_info, NULL, &layout) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create pipeline layout");
        goto exit;
    }

    VkGraphicsPipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = ArrayCount(shader_stages),
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &depth_stencil_state,
        .pColorBlendState = &color_blend_state,
        .layout = layout,
        .renderPass = render_pass,
        .subpass = 0,
    };

    VkPipeline vk_pipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, NULL,
                                  &vk_pipeline) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create graphics pipeline");
        goto exit;
    }

    pipeline_out->vk_pipeline = vk_pipeline;
    pipeline_out->layout = layout;
    pipeline_out->push_constant_size = config->push_constant_size;
    layout = VK_NULL_HANDLE;

    result = true;
    Log(INFO, "Created pipeline [%s + %s]", config->vertex_shader_path, config->fragment_shader_path);

exit:
    if (layout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, layout, NULL);
    if (vertex_shader != VK_NULL_HANDLE)
        vkDestroyShaderModule(device, vertex_shader, NULL);
    if (fragment_shader != VK_NULL_HANDLE)
        vkDestroyShaderModule(device, fragment_shader, NULL);

    MemoryArena_PopTo(arena, scratch_pos);

    return result;
}

void VulkanPipeline_Destroy(VkDevice device, pipeline_t *pipeline)
{
    vkDestroyPipeline(device, pipeline->vk_pipeline, NULL);
    vkDestroyPipelineLayout(device, pipeline->layout, NULL);

    MemoryZeroItem(pipeline);
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

static u8 *read_shader_file(arena_t *arena, const char *path, u64 *size_out)
{
    char full_path[MAX_SHADER_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", SDL_GetBasePath(), path);

    FILE *file = fopen(full_path, "rb");
    if (!file)
    {
        Log(ERROR, "failed to open shader file: %s", full_path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size <= 0)
    {
        Log(ERROR, "failed to read shader file: %s", full_path);
        fclose(file);
        return NULL;
    }

    u8 *data = arena_push_array_no_zero(arena, u8, (u64)size);
    if (fread(data, 1, (u64)size, file) != (u64)size)
    {
        Log(ERROR, "failed to read shader file: %s", full_path);
        fclose(file);
        return NULL;
    }

    fclose(file);

    *size_out = (u64)size;
    return data;
}

static bool create_shader_module(VkDevice device, arena_t *scratch, const char *path,
                                 VkShaderModule *module_out)
{
    u64 size;
    u8 *code = read_shader_file(scratch, path, &size);
    if (!code)
        return false;

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = (const u32 *)code,
    };

    if (vkCreateShaderModule(device, &create_info, NULL, module_out) != VK_SUCCESS)
    {
        Log(ERROR, "failed to create shader module: %s", path);
        return false;
    }

    return true;
}
