#include <stdio.h>

#include <SDL3/SDL_filesystem.h>

#include "file.h"
#include "image.h"
#include "mesh_internal.h"
#include "renderer.h"

#include "vulkan_pass.h"
#include "vulkan_renderer.h"

#define MAX_RESOURCE_PATH 512

static arena_t *s_arena;

bool Renderer_Init(arena_t *arena)
{
    s_arena = arena;

    return true;
}

shader_code_t Renderer_LoadShader(const char *path)
{
    Assert(s_arena != NULL);

    char full_path[MAX_RESOURCE_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", SDL_GetBasePath(), path);

    shader_code_t shader = {0};
    shader.code = File_Read(s_arena, full_path, &shader.size);

    return shader;
}

texture_handle_t Renderer_LoadTexture(const char *path, sampler_handle_t sampler)
{
    char full_path[MAX_RESOURCE_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", SDL_GetBasePath(), path);

    image_t image;
    if (!Image_Load(full_path, &image))
        return TEXTURE_HANDLE_INVALID;

    texture_handle_t texture = VulkanRenderer_CreateTexture(image.width, image.height,
                                                            image.data, sampler);
    Image_Unload(&image);

    return texture;
}

sampler_handle_t Renderer_CreateSampler(void)
{
    return VulkanRenderer_CreateSampler();
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

buffer_object_handle_t Renderer_CreateUniformBuffer(u64 size, uniform_stage_t stage)
{
    return VulkanRenderer_CreateUniformBuffer(size, stage);
}

buffer_object_handle_t Renderer_CreateStorageBuffer(u64 capacity)
{
    return VulkanRenderer_CreateStorageBuffer(capacity);
}

bool Renderer_SetBufferObject(buffer_object_handle_t handle, const void *data, u64 size)
{
    return VulkanRenderer_SetBufferObject(handle, data, size);
}

void Renderer_DrawMesh(renderpass_handle_t pass_handle, pipeline_handle_t pipeline,
                       const void *push_constant_data, mesh_handle_t mesh_handle)
{
    Renderer_DrawMeshInstanced(pass_handle, pipeline, push_constant_data,
                               BUFFER_OBJECT_HANDLE_INVALID, 1, mesh_handle);
}

void Renderer_DrawMeshInstanced(renderpass_handle_t pass_handle, pipeline_handle_t pipeline,
                                const void *push_constant_data,
                                buffer_object_handle_t instance_buffer, u32 instance_count,
                                mesh_handle_t mesh_handle)
{
    const mesh_t *mesh = MeshManager_GetMesh(mesh_handle);

    draw_command_t draw_command = {
        .pass = pass_handle,
        .pipeline = pipeline,
        .push_constant_data = push_constant_data,
        .storage_buffer = instance_buffer,
        .vertex_buffer = mesh->vertex_buffer,
        .index_buffer = mesh->index_buffer,
        .index_count = mesh->index_count,
        .instance_count = instance_count,
    };

    VulkanPass_AddDrawCommand(&draw_command);
}
