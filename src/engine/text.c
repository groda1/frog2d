#include "core.h"
#include "memory_arena.h"
#include "core_math.h"

#include "render_types.h"
#include "renderer.h"

/* std430 layout of 2d_text_ssbo.vert's instance_data */
typedef struct
{
    vec2 position;
    i32  character;
    f32  size;
    vec4 color;
} text_instance_t;

typedef struct
{
    u64 __instance_data_address; /* storage buffer device address, filled in by the renderer */
    u32 texture;
} text_push_constant_t;


typedef struct
{
    arena_t *arena;

    u64 capacity;
    buffer_object_handle_t sbo;

} text_renderer_t;

text_renderer_t s_text_renderer = {};

bool Text_Init(arena_t *arena, u64 initial_character_cap)
{

    s_text_renderer.arena = arena;

    s_text_renderer.sbo = Renderer_CreateStorageBuffer(initial_character_cap);
    if (s_text_renderer.sbo)

    // g_game.text_instances =
    //     Renderer_CreateStorageBuffer(256 * sizeof(text_instance_t));
    // if (g_game.text_instances == BUFFER_OBJECT_HANDLE_INVALID)
    // {
    //     Log(ERROR, "failed to create text instance storage buffer");
    //     Engine_Destroy();
    //     return false;
    // }

    // pipeline_config_t text_pipeline_config = {
    //     .vertex_shader = Renderer_LoadShader("shaders/2d_text_ssbo.vert.spv"),
    //     .fragment_shader = Renderer_LoadShader("shaders/2d_text_ssbo.frag.spv"),
    //     .push_constant_size = sizeof(text_push_constant_t),
    //     .vertex_stride = sizeof(textured_vertex_t),
    //     .vertex_attribute_count = 2,
    //     .vertex_attributes = {
    //         {
    //             .location = 0,
    //             .format = VERTEX_FORMAT_F32X3,
    //             .offset = offsetof(textured_vertex_t, position),
    //         },
    //         {
    //             .location = 1,
    //             .format = VERTEX_FORMAT_F32X2,
    //             .offset = offsetof(textured_vertex_t, texture_coord),
    //         },
    //     },
    //     .uniform_binding_count = 1,
    //     .uniform_bindings = {
    //         {
    //             .binding = 0,
    //             .buffer_object = g_game.vp_uniform_ortho,
    //             .stage = UNIFORM_STAGE_VERTEX,
    //         },
    //     },
    //     .alpha_blending = true,
    // };

    // g_game.text_pipeline = Renderer_AddPipeline(SWAPCHAIN_PASS_HANDLE, &text_pipeline_config);
    // if (g_game.text_pipeline == PIPELINE_HANDLE_INVALID)
    // {
    //     Log(ERROR, "failed to create text pipeline");
    //     Engine_Destroy();
    //     return false;
    // }

    // const char text[] = "foobar FOOBAR <> #!";
    // text_instance_t instances[ArrayCount(text) - 1];
    // for (u32 i = 0; i < ArrayCount(instances); i++)
    // {
    //     instances[i] = (text_instance_t){
    //         .position = V2(1200.0f + (f32)i * 16.0f, 900.0f),
    //         .character = text[i],
    //         .size = 16.0f,
    //         .color = V4(1.0f, 1.0f, 1.0f, 1.0f),
    //     };
    // }
    // Renderer_SetBufferObject(g_game.text_instances, instances, sizeof(instances));
    // g_game.text_instance_count = ArrayCount(instances);


    return true;
}
