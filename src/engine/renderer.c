#include <stdio.h>

#include "core.h"
#include "file.h"
#include "log.h"
#include "mesh.h"
#include "model.h"
#include "os_path.h"
#include "image.h"
#include "mesh_internal.h"
#include "renderer.h"

#include "vulkan_pass.h"
#include "vulkan_renderer.h"
#include "vulkan_buffer.h"

#define MAX_RESOURCE_PATH 512

render_stats_t g_render_stats = {};

extern arena_t *g_engine_arena;

bool Renderer_Init()
{
    return true;
}

shader_code_t Renderer_LoadShader(const char *path)
{
    Assert(g_engine_arena != NULL);

    char full_path[MAX_RESOURCE_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", OS_GetBasePath(), path);

    shader_code_t shader = {0};
    shader.code = File_Read(g_engine_arena, full_path, &shader.size);

    return shader;
}

texture_handle_t Renderer_LoadTexture(const char *path, sampler_handle_t sampler)
{
    char full_path[MAX_RESOURCE_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", OS_GetBasePath(), path);

    image_t image;
    if (!Image_Load(full_path, &image))
        return TEXTURE_HANDLE_INVALID;

    texture_handle_t texture = VulkanRenderer_CreateTexture(image.width, image.height,
                                                            image.data, sampler);
    Image_Unload(&image);

    return texture;
}

texture_handle_t Renderer_CreateTexture(u32 width, u32 height, const u8 *rgba_data,
                                        sampler_handle_t sampler)
{
    return VulkanRenderer_CreateTexture(width, height, rgba_data, sampler);
}

sampler_handle_t Renderer_CreateSampler(void)
{
    return VulkanRenderer_CreateSampler();
}

texture_handle_t Renderer_CreateRenderTexture(u32 width, u32 height, sampler_handle_t sampler)
{
    return VulkanRenderer_CreateRenderTexture(width, height, sampler);
}

renderpass_handle_t Renderer_CreateRenderPass(texture_handle_t target_texture, u32 pass_order)
{
    return VulkanRenderer_CreateRenderPass(target_texture, pass_order);
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
    return VulkanBuffer_SetObjectData(handle, data, size);
}

bool Renderer_ClearBufferObject(buffer_object_handle_t handle)
{
    return VulkanBuffer_ClearObjectData(handle);
}
bool Renderer_PushBufferObject(buffer_object_handle_t handle, const void *data, u64 size)
{
    return VulkanBuffer_PushObjectData(handle, data, size);
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
                                mesh_handle_t mesh)
{
    Assert(mesh != MESH_INVALID_HANDLE);

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

    g_render_stats.n_draw_calls++;
    g_render_stats.n_triangles += instance_count * (mesh->index_count / 3);

    VulkanPass_AddDrawCommand(&draw_command);
}

void Renderer_DrawModel(renderpass_handle_t pass_handle, pipeline_handle_t pipeline,
                       const void *push_constant_data, model_handle_t model)
{
    Assert(model != MODEL_INVALID_HANDLE);

    mesh_handle_t mesh = model->animations[0].keyframes[0].mesh;
    Renderer_DrawMeshInstanced(pass_handle, pipeline, push_constant_data,
                               BUFFER_OBJECT_HANDLE_INVALID, 1, mesh);
}


void Renderer_BeginFrame()
{
    MemoryZeroItem(&g_render_stats);

    VulkanRenderer_BeginFrame();
}

bool Renderer_EndFrame()
{
    return VulkanRenderer_EndFrame();
}

// TODO refactor out VkBuffer
VkBuffer Renderer_CreateStaticVertexBuffer(const void *vertices, u64 size)
{
    VkBuffer buffer = VulkanRenderer_CreateStaticVertexBuffer(vertices, size);
    AssertAlways(buffer != VK_NULL_HANDLE);

    return buffer;
}

// TODO refactor out VkBuffer
VkBuffer Renderer_CreateStaticIndexBuffer(const u32 *indices, u32 index_count)
{
    VkBuffer buffer = VulkanRenderer_CreateStaticIndexBuffer(indices, index_count);
    AssertAlways(buffer != VK_NULL_HANDLE);

    return buffer;
}
