#ifndef RENDERER_H
#define RENDERER_H

#include "memory_arena.h"

#include "mesh.h"
#include "render_types.h"

bool Renderer_Init(arena_t *arena);

window_extent_t Renderer_GetWindowExtent(void);

/* loads a spir-v file relative to the executable directory; the returned code
   lives on the renderer arena and stays valid for the engine's lifetime */
shader_code_t Renderer_LoadShader(const char *path);

pipeline_handle_t Renderer_AddPipeline(renderpass_handle_t pass_handle,
                                       const pipeline_config_t *config);

/* push constant data is copied; the pointer only needs to stay valid for the
   duration of the call */
void Renderer_DrawMesh(renderpass_handle_t pass_handle, pipeline_handle_t pipeline,
                       const void *push_constant_data, const mesh_t *mesh);

#endif
