#ifndef MESH_INTERNAL_H
#define MESH_INTERNAL_H

#include <vulkan/vulkan_core.h>

#include "mesh.h"

/* engine-internal; the gpu buffer handles never leave the engine */

typedef struct _mesh_t mesh_t;
struct _mesh_t
{
    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    u32 index_count;
};

mesh_t *MeshManager_GetMesh(mesh_handle_t handle);

#endif
