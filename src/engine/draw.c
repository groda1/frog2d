#include "core.h"
#include "core_math.h"
#include "engine_types.h"
#include "log.h"

#include "mesh.h"
#include "render_types.h"
#include "renderer.h"

#include "draw.h"

/* layout of 2d_color_ssbo.vert's instance_data */
typedef struct
{
    vec2 position;
    vec2  size;
    vec4 color;
} color_instance_t;

/* layout of 2d_text_ssbo.vert's instance_data */
typedef struct
{
    vec2 position;
    i32  character;
    f32  size;
    vec4 color;
} character_instance_t;

typedef struct
{
    u64 __instance_data_address; /* storage buffer device address, filled in by the renderer */
} color_push_constant_t;

typedef struct
{
    u64 __instance_data_address; /* storage buffer device address, filled in by the renderer */
    u32 texture;
} text_push_constant_t;


typedef struct
{


    buffer_object_handle_t vp_uniform;
    mesh_handle_t quad_mesh;

    // Simple mesh drawing
    u64 colored_sbo_capacity;
    u64 colored_sbo_len;
    buffer_object_handle_t colored_sbo;
    pipeline_handle_t colored_pipeline;

    // Text drawing
    u64 text_sbo_capacity;
    u64 text_sbo_len;
    buffer_object_handle_t text_sbo;
    pipeline_handle_t text_pipeline;
    texture_handle_t font_texture;
    vec4 char_color;
    u32 char_size;

} draw_renderer_t;

static draw_renderer_t s_draw = {};

bool Draw_Init()
{
    s_draw.vp_uniform = Renderer_CreateUniformBuffer(sizeof(view_projection_t), UNIFORM_STAGE_VERTEX);
    if (s_draw.vp_uniform == BUFFER_OBJECT_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create view projection uniform");
        return false;
    }

    // Colored meshes pipeline
    s_draw.colored_sbo_capacity = 2048;
    s_draw.colored_sbo = Renderer_CreateStorageBuffer( s_draw.colored_sbo_capacity * sizeof(color_instance_t));
    if (s_draw.colored_sbo == BUFFER_OBJECT_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create colored mesh renderer SBO");
        return false;
    }

    pipeline_config_t colored_pipeline_config = {
        .name = "colored-draw-renderer",
        .vertex_shader = Renderer_LoadShader("shaders/2d_color_ssbo.vert.spv"),
        .fragment_shader = Renderer_LoadShader("shaders/2d_color_ssbo.frag.spv"),
        .push_constant_size = sizeof(color_push_constant_t),
        .vertex_stride = sizeof(simple_vertex_t),
        .vertex_attribute_count = 1,
        .vertex_attributes = {
            {
                .location = 0,
                .format = VERTEX_FORMAT_F32X3,
                .offset = offsetof(simple_vertex_t, position),
            },
        },
        .uniform_binding_count = 1,
        .uniform_bindings = {
            {
                .binding = 0,
                .buffer_object = s_draw.vp_uniform,
                .stage = UNIFORM_STAGE_VERTEX,
            },
        },
        .alpha_blending = true,
        .disable_depth_test = true,
    };

    s_draw.colored_pipeline = Renderer_AddPipeline(SWAPCHAIN_PASS_HANDLE, &colored_pipeline_config);
    if (s_draw.colored_pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to colored mesh pipeline");
        return false;
    }

    // Text pipeline
    s_draw.text_sbo_capacity = 8192;
    s_draw.text_sbo = Renderer_CreateStorageBuffer(s_draw.text_sbo_capacity * sizeof(character_instance_t));
    if (s_draw.text_sbo == BUFFER_OBJECT_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create text renderer SBO");
        return false;
    }

    sampler_handle_t sampler = Renderer_CreateSampler();
    if (sampler == SAMPLER_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create sampler");
        return false;
    }

    s_draw.font_texture = Renderer_LoadTexture("resources/textures/font2.png", sampler);
    if (s_draw.font_texture == TEXTURE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to load font texture");
        return false;
    }

    pipeline_config_t text_pipeline_config = {
        .name = "text-renderer",
        .vertex_shader = Renderer_LoadShader("shaders/2d_text_ssbo.vert.spv"),
        .fragment_shader = Renderer_LoadShader("shaders/2d_text_ssbo.frag.spv"),
        .push_constant_size = sizeof(text_push_constant_t),
        .vertex_stride = sizeof(textured_vertex_t),
        .vertex_attribute_count = 2,
        .vertex_attributes = {
            {
                .location = 0,
                .format = VERTEX_FORMAT_F32X3,
                .offset = offsetof(textured_vertex_t, position),
            },
            {
                .location = 1,
                .format = VERTEX_FORMAT_F32X2,
                .offset = offsetof(textured_vertex_t, texture_coord),
            },
        },
        .uniform_binding_count = 1,
        .uniform_bindings = {
            {
                .binding = 0,
                .buffer_object = s_draw.vp_uniform,
                .stage = UNIFORM_STAGE_VERTEX,
            },
        },
        .alpha_blending = true,
        .disable_depth_test = true,
    };

    s_draw.text_pipeline = Renderer_AddPipeline(SWAPCHAIN_PASS_HANDLE, &text_pipeline_config);
    if (s_draw.text_pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create text pipeline");
        return false;
    }

    window_extent_t extent = Renderer_GetWindowExtent();
    Draw_HandleResize(extent.width, extent.height);

    Draw_SetTextColor(V4(1.0, 1.0, 1.0, 1.0));
    Draw_SetTextSize(8);

    Log(INFO, "Draw renderer initialized");
    return true;
}

void Draw_Destroy()
{
    Log(INFO, "Draw renderer destroyed");
}

void Draw_HandleResize(u32 width, u32 height)
{
    view_projection_t vp = {
        .view = HMM_M4D(1.0f),
        .proj = HMM_Orthographic_RH_NO(0.0f, (f32)width, 0.0f, (f32)height, -1.0, 1.0)
    };
    Renderer_SetBufferObject(s_draw.vp_uniform, &vp, sizeof(vp));
}

bool Draw_Quad(u32 x, u32 y, u32 width, u32 height, vec4 color)
{
    color_instance_t instance = {
        .position = V2((f32)x + ((f32)width/2.0), (f32)y + ((f32)height/ 2.0)),
        .size = V2((f32)width, (f32)height),
        .color = color
    };
    Renderer_PushBufferObject(s_draw.colored_sbo, &instance, sizeof(instance));
    s_draw.colored_sbo_len++;

    return true;
}

void Draw_SetTextSize(u32 size)
{
    s_draw.char_size = size;
}

void Draw_SetTextColor(vec4 color)
{
    s_draw.char_color = color;
}

bool Draw_Text(u32 x, u32 y, string text)
{
    for (u32 i = 0; i < text.len; i++)
    {
        character_instance_t character = {
            .position = V2((f32)x + (f32)(i * s_draw.char_size) + (f32)s_draw.char_size / 2, (f32)y + (f32)s_draw.char_size),
            .character = text.str[i],
            .size = (f32)s_draw.char_size,
            .color = s_draw.char_color,
        };

        Renderer_PushBufferObject(s_draw.text_sbo, &character, sizeof(character));
        s_draw.text_sbo_len++;
    }

    return true;
}

void Draw_BeginFrame()
{
    Renderer_ClearBufferObject(s_draw.colored_sbo);
    Renderer_ClearBufferObject(s_draw.text_sbo);

    s_draw.colored_sbo_len = 0;
    s_draw.text_sbo_len = 0;
}

void Draw_EndFrame()
{
    if (s_draw.colored_sbo_len)
    {
        color_push_constant_t color_push_constant = {};
        Renderer_DrawMeshInstanced(SWAPCHAIN_PASS_HANDLE,
            s_draw.colored_pipeline,
            &color_push_constant,
            s_draw.colored_sbo,
            s_draw.colored_sbo_len,
            PREDEFINED_MESH_SIMPLE_QUAD);
    }

    if (s_draw.text_sbo_len)
    {
        text_push_constant_t text_push_constant = {
            .texture = s_draw.font_texture
        };
        Renderer_DrawMeshInstanced(SWAPCHAIN_PASS_HANDLE, s_draw.text_pipeline,
            &text_push_constant,
            s_draw.text_sbo,
            s_draw.text_sbo_len,
            PREDEFINED_MESH_TEXTURED_QUAD);
    }
}
