#include "core.h"
#include "core_math.h"
#include "engine_types.h"
#include "memory_arena.h"
#include "log.h"

#include "render_types.h"
#include "renderer.h"

#include "text.h"

/* std430 layout of 2d_text_ssbo.vert's instance_data */
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
    u32 texture;
} text_push_constant_t;


typedef struct
{
    arena_t *arena;

    u64 capacity;
    u64 buf_len;
    character_instance_t *buf;

    buffer_object_handle_t sbo;
    buffer_object_handle_t vp_uniform;
    pipeline_handle_t pipeline;
    texture_handle_t font_texture;
    mesh_handle_t quad_mesh;

    vec4 char_color;
    u32 char_size;

} text_renderer_t;

static text_renderer_t s_text_renderer = {};

bool Text_Init(arena_t *arena, u64 initial_character_cap)
{

    s_text_renderer.arena = arena;
    s_text_renderer.capacity = initial_character_cap;
    s_text_renderer.buf = arena_push_array_no_zero(arena, character_instance_t, initial_character_cap);

    s_text_renderer.sbo = Renderer_CreateStorageBuffer(initial_character_cap * sizeof(character_instance_t));
    if (s_text_renderer.sbo == BUFFER_OBJECT_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create text renderer SBO");
        return false;
    }

    s_text_renderer.vp_uniform = Renderer_CreateUniformBuffer(sizeof(view_projection_t), UNIFORM_STAGE_VERTEX);
    if (s_text_renderer.vp_uniform == BUFFER_OBJECT_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create view projection uniform");
        return false;
    }

    sampler_handle_t sampler = Renderer_CreateSampler();
    if (sampler == SAMPLER_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create sampler");
        return false;
    }

    s_text_renderer.font_texture = Renderer_LoadTexture("resources/textures/font2.png", sampler);
    if (s_text_renderer.font_texture == TEXTURE_HANDLE_INVALID)
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
                .buffer_object = s_text_renderer.vp_uniform,
                .stage = UNIFORM_STAGE_VERTEX,
            },
        },
        .alpha_blending = true,
        .disable_depth_test = true,
    };

    s_text_renderer.pipeline = Renderer_AddPipeline(SWAPCHAIN_PASS_HANDLE, &text_pipeline_config);
    if (s_text_renderer.pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create text pipeline");
        return false;
    }

    window_extent_t extent = Renderer_GetWindowExtent();
    Text_HandleResize(extent.width, extent.height);

    Text_SetColor(V4(1.0, 1.0, 1.0, 1.0));
    Text_SetSize(8);

    Log(INFO, "Text renderer initialized");
    return true;
}

void Text_Destroy()
{
    Log(INFO, "Text renderer destroyed");
}

void Text_HandleResize(u32 width, u32 height)
{
    view_projection_t vp = {
        .view = HMM_M4D(1.0f),
        .proj = HMM_Orthographic_RH_NO(0.0f, (f32)width, 0.0f, (f32)height, -1.0, 1.0)
    };
    Renderer_SetBufferObject(s_text_renderer.vp_uniform, &vp, sizeof(vp));
}

void Text_SetSize(u32 size)
{
    s_text_renderer.char_size = size;
}

void Text_SetColor(vec4 color)
{
    s_text_renderer.char_color = color;
}

bool Text_Draw(u32 x, u32 y, string text)
{
    if ((text.len + s_text_renderer.buf_len) > s_text_renderer.capacity)
    {
        // TODO grow SBO
        Log(WARNING, "text renderer capacity exceeded");
        return false;
    }
    for (u32 i = 0; i < text.len; i++)
    {
        character_instance_t character = {
            .position = V2((f32)x + (f32)(i * s_text_renderer.char_size) + (f32)s_text_renderer.char_size / 2, (f32)y + (f32)s_text_renderer.char_size),
            .character = text.str[i],
            .size = (f32)s_text_renderer.char_size,
            .color = s_text_renderer.char_color,
        };

        s_text_renderer.buf[s_text_renderer.buf_len++] = character;
    }

    return true;
}

void Text_BeginFrame()
{
    s_text_renderer.buf_len = 0;
    // TODO clear bufferobject and push it durign draw.
}

void Text_EndFrame()
{
    if (s_text_renderer.buf_len == 0)
        return;

    Renderer_SetBufferObject(s_text_renderer.sbo, s_text_renderer.buf,
                             s_text_renderer.buf_len * sizeof(character_instance_t));

    text_push_constant_t text_push_constant = {
        .texture = s_text_renderer.font_texture
    };

    Renderer_DrawMeshInstanced(SWAPCHAIN_PASS_HANDLE, s_text_renderer.pipeline,
        &text_push_constant,
        s_text_renderer.sbo,
        s_text_renderer.buf_len,
        PREDEFINED_MESH_TEXTURED_QUAD);
}
