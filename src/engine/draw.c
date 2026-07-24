#include "core.h"
#include "core_math.h"
#include "engine_types.h"
#include "log.h"

#include "mesh.h"
#include "render_types.h"
#include "renderer.h"

#include "draw.h"

#define FONT_ATLAS_COLUMNS    16
#define FONT_ATLAS_ROWS       6
#define FONT_ATLAS_FIRST_CHAR 32

typedef struct
{
    vec2 position;
    vec2 size;
    vec2 uv_min;
    vec2 uv_max;
    vec4 color;
    texture_handle_t texture;
    u32  text;
} quad_instance_t;
StaticAssert(sizeof(quad_instance_t) == 64, "quad_instance_t must match the shader's std430 stride");

typedef struct
{
    sbo_push_constant_t sbo;
} push_constant_t;


typedef struct
{
    buffer_object_handle_t vp_uniform;
    mesh_handle_t quad_mesh;

    u64 sbo_capacity;
    u64 sbo_len;
    buffer_object_handle_t sbo;
    pipeline_handle_t pipeline;

    texture_handle_t white_texture;
    texture_handle_t font_texture;

    vec4 char_color;
    u32 char_size;

} draw_renderer_t;

static draw_renderer_t s_draw = {};

bool Draw_Init()
{
    s_draw.quad_mesh = MeshManager_GetPredefinedMesh(PREDEFINED_MESH_TEXTURED_QUAD);

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
        Log(ERROR, "failed to create 2d renderer SBO");
        return false;
    }

    sampler_handle_t sampler = Renderer_CreateSampler();
    if (sampler == SAMPLER_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create sampler");
        return false;
    }

    static const u8 white_pixel[4] = {255, 255, 255, 255};
    s_draw.white_texture = Renderer_CreateTexture(1, 1, white_pixel, sampler);
    if (s_draw.white_texture == TEXTURE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create white texture");
        return false;
    }

    s_draw.font_texture = Renderer_LoadTexture("resources/textures/font2.png", sampler);
    if (s_draw.font_texture == TEXTURE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to load font texture");
        return false;
    }

    pipeline_config_t pipeline_config = {
        .name = "2d-renderer",
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

    s_draw.pipeline = Renderer_AddPipeline(SWAPCHAIN_PASS_HANDLE, &pipeline_config);
    if (s_draw.pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create 2d pipeline");
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
    return Draw_TexturedQuad(x, y, width, height, color, s_draw.white_texture);
}

bool Draw_TexturedQuad(u32 x, u32 y, u32 width, u32 height, vec4 color, texture_handle_t texture)
{
    quad_instance_t quad = {
        .position = V2((f32)x + ((f32)width/2.0), (f32)y + ((f32)height/ 2.0)),
        .size = V2((f32)width, (f32)height),
        .uv_min = V2(0.0f, 0.0f),
        .uv_max = V2(1.0f, 1.0f),
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
        u32 cell = (u32)text.str[i] - FONT_ATLAS_FIRST_CHAR;
        f32 u0 = (f32)(cell % FONT_ATLAS_COLUMNS) / FONT_ATLAS_COLUMNS;
        f32 v0 = (f32)(cell / FONT_ATLAS_COLUMNS) / FONT_ATLAS_ROWS;

        quad_instance_t character = {
            .position = V2((f32)x + (f32)(i * s_draw.char_size) + (f32)s_draw.char_size / 2, (f32)y + (f32)s_draw.char_size),
            .size = V2((f32)s_draw.char_size, (f32)s_draw.char_size * 2),
            .uv_min = V2(u0, v0),
            .uv_max = V2(u0 + 1.0f / FONT_ATLAS_COLUMNS, v0 + 1.0f / FONT_ATLAS_ROWS),
            .color = s_draw.char_color,
            .texture = s_draw.font_texture,
            .text = 1,
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
            s_draw.quad_mesh);
    }
}
