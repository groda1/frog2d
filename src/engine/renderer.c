#include "renderer.h"

#include "vulkan_pass.h"
#include "vulkan_renderer.h"

window_extent_t Renderer_GetWindowExtent(void)
{
    VkExtent2D extent = VulkanRenderer_GetExtent();

    return (window_extent_t){
        .width = extent.width,
        .height = extent.height,
    };
}

pipeline_handle_t Renderer_AddPipeline(renderpass_handle_t pass_handle,
                                       const pipeline_config_t *config)
{
    return VulkanRenderer_AddPipeline(pass_handle, config);
}

void Renderer_DrawMesh(renderpass_handle_t pass_handle, pipeline_handle_t pipeline,
                       const void *push_constant_data, const mesh_t *mesh)
{
    draw_command_t draw_command = {
        .pass = pass_handle,
        .pipeline = pipeline,
        .push_constant_data = push_constant_data,
        .vertex_buffer = mesh->vertex_buffer,
        .index_buffer = mesh->index_buffer,
        .index_count = mesh->index_count,
    };

    VulkanPass_AddDrawCommand(&draw_command);
}
