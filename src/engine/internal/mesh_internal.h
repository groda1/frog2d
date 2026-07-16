#ifndef MESH_INTERNAL_H
#define MESH_INTERNAL_H

#include <vulkan/vulkan_core.h>

#include "core.h"

struct _mesh_t
{
    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    u32 index_count;
};

#endif
