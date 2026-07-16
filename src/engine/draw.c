#include "core.h"
#include "core_math.h"
#include "engine_types.h"
#include "log.h"

#include "mesh.h"
#include "render_types.h"
#include "renderer.h"

#include "draw.h"

#define COLORED_QUAD -1
#define TEXTURED_QUAD -2

/* layout of 2d_text_ssbo.vert's instance_data */
typedef struct
{
    vec2 position;
    vec2 size;
    i32  character;
    vec4 color;
    texture_handle_t texture;
} quad_instance_t;


typedef struct
{
    u64 __instance_data_address; /* storage buffer device address, filled in by the renderer */
} push_constant_t;


typedef struct
{
    buffer_object_handle_t vp_uniform;
    mesh_handle_t quad_mesh;
    mesh_handle_t textured_quad_mesh;

    u64 sbo_capacity;
    u64 sbo_len;
    buffer_object_handle_t sbo;
    pipeline_handle_t pipeline;

    texture_handle_t font_texture;
    vec4 char_color;
    u32 char_size;

} draw_renderer_t;

static draw_renderer_t s_draw = {};

bool Draw_Init()
{
    s_draw.quad_mesh = MeshManager_GetPredefinedMesh(PREDEFINED_MESH_SIMPLE_QUAD);
    s_draw.textured_quad_mesh = MeshManager_GetPredefinedMesh(PREDEFINED_MESH_TEXTURED_QUAD);

    s_draw.vp_uniform = Renderer_CreateUniformBuffer(sizeof(view_projection_t), UNIFORM_STAGE_VERTEX);
    if (s_draw.vp_uniform == BUFFER_OBJECT_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create view projection uniform");
        return false;
    }

    s_draw.sbo_capacity = 8192;
    s_draw.sbo = Renderer_CreateStorageBuffer(s_draw.sbo_capacity * sizeof(quad_instance_t));
    if (s_draw.sbo == BUFFER_OBJECT_HANDLE_INVALID)
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
        .vertex_shader = Renderer_LoadShader("shaders/2d_ssbo.vert.spv"),
        .fragment_shader = Renderer_LoadShader("shaders/2d_ssbo.frag.spv"),
        .push_constant_size = sizeof(push_constant_t),
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

    s_draw.pipeline = Renderer_AddPipeline(SWAPCHAIN_PASS_HANDLE, &text_pipeline_config);
    if (s_draw.pipeline == PIPELINE_HANDLE_INVALID)
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
    quad_instance_t quad = {
        .position = V2((f32)x + ((f32)width/2.0), (f32)y + ((f32)height/ 2.0)),
        .character = COLORED_QUAD,
        .size = V2((f32)width, (f32)height),
        .color = color,
        .texture = 0,
    };

    Renderer_PushBufferObject(s_draw.sbo, &quad, sizeof(quad));
    s_draw.sbo_len++;

    return true;
}

bool Draw_TexturedQuad(u32 x, u32 y, u32 width, u32 height, vec4 color, texture_handle_t texture)
{
    (void)texture;

    quad_instance_t quad = {
        .position = V2((f32)x + ((f32)width/2.0), (f32)y + ((f32)height/ 2.0)),
        .character = TEXTURED_QUAD,
        .size = V2((f32)width, (f32)height),
        .color = color,
        .texture = texture,
    };

    Renderer_PushBufferObject(s_draw.sbo, &quad, sizeof(quad));
    s_draw.sbo_len++;

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
        quad_instance_t character = {
            .position = V2((f32)x + (f32)(i * s_draw.char_size) + (f32)s_draw.char_size / 2, (f32)y + (f32)s_draw.char_size),
            .character = text.str[i],
            .size = V2((f32)s_draw.char_size, (f32)s_draw.char_size * 2),
            .color = s_draw.char_color,
            .texture = s_draw.font_texture,
        };

        Renderer_PushBufferObject(s_draw.sbo, &character, sizeof(character));
        s_draw.sbo_len++;
    }

    return true;
}

void Draw_BeginFrame()
{
    Renderer_ClearBufferObject(s_draw.sbo);

    s_draw.sbo_len = 0;
}

void Draw_EndFrame()
{
    if (s_draw.sbo_len)
    {
        push_constant_t push_constant = {};

        Renderer_DrawMeshInstanced(SWAPCHAIN_PASS_HANDLE, s_draw.pipeline,
            &push_constant,
            s_draw.sbo,
            s_draw.sbo_len,
            s_draw.textured_quad_mesh);
    }
}
