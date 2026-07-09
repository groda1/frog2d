#include <stdio.h>

#include <SDL3/SDL_filesystem.h>

#include "file.h"
#include "renderer.h"

#include "vulkan_pass.h"
#include "vulkan_renderer.h"

#define MAX_SHADER_PATH 512

static arena_t *s_arena;

bool Renderer_Init(arena_t *arena)
{
    s_arena = arena;

    return true;
}

shader_code_t Renderer_LoadShader(const char *path)
{
    Assert(s_arena != NULL);

    char full_path[MAX_SHADER_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", SDL_GetBasePath(), path);

    shader_code_t shader = {0};
    shader.code = File_Read(s_arena, full_path, &shader.size);

    return shader;
}

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
