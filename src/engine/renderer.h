#ifndef RENDERER_H
#define RENDERER_H

#include "memory_arena.h"

#include "mesh.h"
#include "render_types.h"

bool Renderer_Init(arena_t *arena);

window_extent_t Renderer_GetWindowExtent(void);

shader_code_t Renderer_LoadShader(const char *path);

/* the returned handle indexes the global texture array in shaders */
texture_handle_t Renderer_LoadTexture(const char *path, sampler_handle_t sampler);
sampler_handle_t Renderer_CreateSampler(void);

/* a texture that a render pass draws into; sampled like any loaded texture */
texture_handle_t Renderer_CreateRenderTexture(u32 width, u32 height, sampler_handle_t sampler);

/* a pass rendering into a render texture, cleared to transparent black each
   frame; passes render in ascending pass_order before the swapchain pass, so
   any pass can sample the render textures of the passes before it */
renderpass_handle_t Renderer_CreateRenderPass(texture_handle_t target_texture, u32 pass_order);

pipeline_handle_t Renderer_AddPipeline(renderpass_handle_t pass_handle,
                                       const pipeline_config_t *config);

buffer_object_handle_t Renderer_CreateUniformBuffer(u64 size, uniform_stage_t stage);

/* storage buffer; like textures, needs no pipeline configuration: shaders
   reach it through its device address (GL_EXT_buffer_reference), passed in
   the push constant per draw. capacity is the initial size; Set/Push grow
   the buffer on demand */
buffer_object_handle_t Renderer_CreateStorageBuffer(u64 capacity);

/* the data is copied into the buffer object's cpu shadow and uploaded to the
   gpu buffers over the next frames; the pointer only needs to stay valid for
   the duration of the call */
bool Renderer_SetBufferObject(buffer_object_handle_t handle, const void *data, u64 size);
bool Renderer_ClearBufferObject(buffer_object_handle_t handle);
bool Renderer_PushBufferObject(buffer_object_handle_t handle, const void *data, u64 size);

/* push constant data is copied; the pointer only needs to stay valid for the
   duration of the call */
void Renderer_DrawMesh(renderpass_handle_t pass_handle, pipeline_handle_t pipeline,
                       const void *push_constant_data, mesh_handle_t mesh);

/* draws instance_count instances; if instance_buffer is a valid storage
   buffer handle, the renderer writes its device address into the first 8
   bytes of the push constant, so the push constant struct must start with a
   u64 placeholder */
void Renderer_DrawMeshInstanced(renderpass_handle_t pass_handle, pipeline_handle_t pipeline,
                                const void *push_constant_data,
                                buffer_object_handle_t instance_buffer, u32 instance_count,
                                mesh_handle_t mesh);

#endif
