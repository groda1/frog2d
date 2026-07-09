#ifndef RENDER_TYPES_H
#define RENDER_TYPES_H

#include "core.h"
#include "core_string.h"

/* engine-side rendering types; implemented by the vulkan renderer */

#define SWAPCHAIN_PASS_HANDLE   U32_MAX
#define PIPELINE_HANDLE_INVALID U32_MAX

#define MAX_VERTEX_ATTRIBUTES 8

typedef u32 renderpass_handle_t;
typedef u32 pipeline_handle_t;

typedef struct
{
    u32 width;
    u32 height;
} window_extent_t;

typedef enum
{
    VERTEX_FORMAT_F32 = 0,
    VERTEX_FORMAT_F32X2,
    VERTEX_FORMAT_F32X3,
    VERTEX_FORMAT_F32X4,
} vertex_format_t;

typedef struct
{
    u32             location;
    vertex_format_t format;
    u32             offset;
} vertex_attribute_t;

typedef struct _pipeline_config_t pipeline_config_t;
struct _pipeline_config_t
{
    string vertex_shader_path;
    string fragment_shader_path;

    u32 push_constant_size; // 0 = no push constants; vertex stage only for now

    u32 vertex_stride;
    u32 vertex_attribute_count;
    vertex_attribute_t vertex_attributes[MAX_VERTEX_ATTRIBUTES];

    bool alpha_blending;

    // TODO uniform buffers / samplers (descriptor set layouts)
    // TODO vertex topology (always triangle list for now)
};

#endif
