#ifndef RENDERER_H
#define RENDERER_H

#include "core.h"

#include "mesh.h"
#include "render_types.h"

window_extent_t Renderer_GetWindowExtent(void);

pipeline_handle_t Renderer_AddPipeline(renderpass_handle_t pass_handle,
                                       const pipeline_config_t *config);

/* push constant data is read when the frame is baked and must stay valid
   until the end of the engine tick */
void Renderer_DrawMesh(renderpass_handle_t pass_handle, pipeline_handle_t pipeline,
                       const void *push_constant_data, const mesh_t *mesh);

#endif
