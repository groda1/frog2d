#include "HandmadeMath.h"
#include "core.h"
#include "core_math.h"
#include "log.h"

#include "engine_main.h"
#include "game_main.h"
#include "mesh.h"
#include "renderer.h"


/* port of the vulkrap hello_krap example: a wobbling triangle */

#define ROT_SPEED_DEG_PER_S      25.0f
#define WOBBLE_SPEED             5.0f
#define CUBE_ROT_SPEED_DEG_PER_S 40.0f

#define MAX_TEXT_INSTANCES       256

typedef struct
{
    mat4 transform;
    f32 wobble;
} push_constant_t;

typedef struct
{
    mat4 transform;
    vec4 color;
} flat_push_constant_t;

typedef struct
{
    mat4 transform;
    vec4 color;
    u32 texture;
} textured_push_constant_t;

typedef struct
{
    mat4 view;
    mat4 proj;
} view_projection_t;

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
    u64 instance_data; /* storage buffer device address, written by the renderer */
    u32 texture;
} text_push_constant_t;

typedef struct
{
    mesh_handle_t mesh;
    vec3 position;
    quat orientation;
    f32 wobble;
    pipeline_handle_t pipeline;

    /* 3d cube rendered behind the triangle */
    mesh_handle_t cube_mesh;
    quat cube_orientation;
    pipeline_handle_t cube_pipeline;
    buffer_object_handle_t vp_uniform;

    /* textured quad between the cubes */
    mesh_handle_t quad_mesh;
    texture_handle_t quad_texture;
    texture_handle_t font_texture;
    pipeline_handle_t quad_pipeline;
    buffer_object_handle_t vp_uniform_ortho;

    /* instanced text: per-instance data in a storage buffer reached by
       device address */
    pipeline_handle_t text_pipeline;
    buffer_object_handle_t text_instances;
    u32 text_instance_count;
} game_t;

static game_t g_game;

bool Game_Init(SDL_Window *window)
{
    if (!Engine_Init(window))
        return false;

    g_game.mesh = PREDEFINED_MESH_COLORED_TRIANGLE;

    window_extent_t extent = Renderer_GetWindowExtent();
    g_game.position = V3((f32)extent.width / 2.0f, (f32)extent.height / 2.0f, 0.0f);
    g_game.orientation = HMM_Q(0.0f, 0.0f, 0.0f, 1.0f);

    pipeline_config_t pipeline_config = {
        .vertex_shader = Renderer_LoadShader("shaders/hello_triangle.vert.spv"),
        .fragment_shader = Renderer_LoadShader("shaders/hello_triangle.frag.spv"),
        .push_constant_size = sizeof(push_constant_t),
        .vertex_stride = sizeof(colored_vertex_t),
        .vertex_attribute_count = 2,
        .vertex_attributes = {
            {
                .location = 0,
                .format = VERTEX_FORMAT_F32X3,
                .offset = offsetof(colored_vertex_t, position),
            },
            {
                .location = 1,
                .format = VERTEX_FORMAT_F32X3,
                .offset = offsetof(colored_vertex_t, color),
            },
        },
    };

    g_game.pipeline = Renderer_AddPipeline(SWAPCHAIN_PASS_HANDLE, &pipeline_config);
    if (g_game.pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create hello triangle pipeline");
        Engine_Destroy();
        return false;
    }

    /* cube */
    g_game.cube_mesh = PREDEFINED_MESH_NORMALED_CUBE;
    g_game.cube_orientation = HMM_Q(0.0f, 0.0f, 0.0f, 1.0f);

    g_game.vp_uniform = Renderer_CreateUniformBuffer(sizeof(view_projection_t),
                                                     UNIFORM_STAGE_VERTEX);


    if (g_game.vp_uniform == BUFFER_OBJECT_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create view projection uniform");
        Engine_Destroy();
        return false;
    }

    pipeline_config_t cube_pipeline_config = {
        .vertex_shader = Renderer_LoadShader("shaders/flat_color_edge.vert.spv"),
        .fragment_shader = Renderer_LoadShader("shaders/flat_color_edge.frag.spv"),
        .push_constant_size = sizeof(flat_push_constant_t),
        .vertex_stride = sizeof(normal_vertex_t),
        .vertex_attribute_count = 1,
        .vertex_attributes = {
            {
                .location = 0,
                .format = VERTEX_FORMAT_F32X3,
                .offset = offsetof(normal_vertex_t, position),
            },
        },
        .uniform_binding_count = 1,
        .uniform_bindings = {
            {
                .binding = 0,
                .buffer_object = g_game.vp_uniform,
                .stage = UNIFORM_STAGE_VERTEX,
            },
        },
    };

    g_game.cube_pipeline = Renderer_AddPipeline(SWAPCHAIN_PASS_HANDLE, &cube_pipeline_config);
    if (g_game.cube_pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create flat color pipeline");
        Engine_Destroy();
        return false;
    }

    /* textured quad */
    g_game.quad_mesh = PREDEFINED_MESH_TEXTURED_QUAD;

    g_game.vp_uniform_ortho = Renderer_CreateUniformBuffer(sizeof(view_projection_t), UNIFORM_STAGE_VERTEX);

    sampler_handle_t sampler = Renderer_CreateSampler();
    if (sampler == SAMPLER_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create sampler");
        Engine_Destroy();
        return false;
    }

    g_game.quad_texture = Renderer_LoadTexture("resources/textures/test.png", sampler);
    if (g_game.quad_texture == TEXTURE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to load test texture");
        Engine_Destroy();
        return false;
    }

    g_game.font_texture = Renderer_LoadTexture("resources/textures/font2.png", sampler);
    if (g_game.font_texture == TEXTURE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to load font texture");
        Engine_Destroy();
        return false;
    }

    pipeline_config_t quad_pipeline_config = {
        .vertex_shader = Renderer_LoadShader("shaders/flat_textured.vert.spv"),
        .fragment_shader = Renderer_LoadShader("shaders/flat_textured.frag.spv"),
        .push_constant_size = sizeof(textured_push_constant_t),
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
                .buffer_object = g_game.vp_uniform_ortho,
                .stage = UNIFORM_STAGE_VERTEX,
            },
        },
    };

    g_game.quad_pipeline = Renderer_AddPipeline(SWAPCHAIN_PASS_HANDLE, &quad_pipeline_config);
    if (g_game.quad_pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create flat textured pipeline");
        Engine_Destroy();
        return false;
    }

    view_projection_t vp2 = {
        .view = HMM_M4D(1.0f),
        .proj = HMM_Orthographic_RH_NO(0.0f, (f32)extent.width, 0.0f, (f32)extent.height, -1.0, 1.0)
    };
    Renderer_SetBufferObject(g_game.vp_uniform_ortho, &vp2, sizeof(vp2));

    /* instanced text */
    g_game.text_instances =
        Renderer_CreateStorageBuffer(256 * sizeof(text_instance_t));
    if (g_game.text_instances == BUFFER_OBJECT_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create text instance storage buffer");
        Engine_Destroy();
        return false;
    }

    pipeline_config_t text_pipeline_config = {
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
                .buffer_object = g_game.vp_uniform_ortho,
                .stage = UNIFORM_STAGE_VERTEX,
            },
        },
        .alpha_blending = true,
    };

    g_game.text_pipeline = Renderer_AddPipeline(SWAPCHAIN_PASS_HANDLE, &text_pipeline_config);
    if (g_game.text_pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create text pipeline");
        Engine_Destroy();
        return false;
    }

    const char text[] = "foobar FOOBAR <> #!";
    text_instance_t instances[ArrayCount(text) - 1];
    for (u32 i = 0; i < ArrayCount(instances); i++)
    {
        instances[i] = (text_instance_t){
            .position = V2(1200.0f + (f32)i * 16.0f, 900.0f),
            .character = text[i],
            .size = 16.0f,
            .color = V4(1.0f, 1.0f, 1.0f, 1.0f),
        };
    }
    Renderer_SetBufferObject(g_game.text_instances, instances, sizeof(instances));
    g_game.text_instance_count = ArrayCount(instances);

    return true;
}

void Game_Destroy(void)
{
    Engine_Destroy();
}

void Game_HandleKeyDown(SDL_Keycode key)
{
    (void)key;
}

void Game_HandleKeyUp(SDL_Keycode key)
{
    (void)key;
}

void Game_HandleResize(u32 width, u32 height)
{
    Engine_HandleResize(width, height);

    window_extent_t extent = Renderer_GetWindowExtent();
    view_projection_t vp2 = {
        .view = HMM_M4D(1.0f),
        .proj = HMM_Orthographic_RH_NO(0.0f, (f32)extent.width, 0.0f, (f32)extent.height, -1.0, 1.0)
    };
    Renderer_SetBufferObject(g_game.vp_uniform_ortho, &vp2, sizeof(vp2));

}

void Game_Tick(void)
{
    f32 delta_time = Engine_BeginFrame();

    window_extent_t extent = Renderer_GetWindowExtent();

    push_constant_t push_constant;

    quat rotation = HMM_QFromAxisAngle_RH(V3(0.0f, 0.0f, 1.0f),
                                          HMM_AngleDeg(-delta_time * ROT_SPEED_DEG_PER_S));

    g_game.orientation = HMM_MulQ(g_game.orientation, rotation);
    g_game.wobble += delta_time * WOBBLE_SPEED;
    push_constant.wobble = g_game.wobble;
    g_game.position = V3(((f32)extent.width / 2.0f) - 250, (f32)extent.height / 2.0f, 0.0f);

    mat4 projection = HMM_Orthographic_RH_NO(0.0f, (f32)extent.width,
                                             0.0f, (f32)extent.height, -1.0f, 1.0f);
    f32 size = extent.height / 2;
    mat4 model = HMM_MulM4(HMM_Translate(g_game.position),
                           HMM_MulM4(HMM_QToM4(g_game.orientation),
                                     HMM_Scale(V3(size, size, size))));
    push_constant.transform = HMM_MulM4(projection, model);


    Renderer_DrawMesh(SWAPCHAIN_PASS_HANDLE, g_game.pipeline, &push_constant, g_game.mesh);

    /* update cube */
    quat cube_rotation = HMM_MulQ(
        HMM_QFromAxisAngle_RH(V3(0.0f, 1.0f, 0.0f),
                              HMM_AngleDeg(delta_time * CUBE_ROT_SPEED_DEG_PER_S)),
        HMM_QFromAxisAngle_RH(V3(1.0f, 0.0f, 0.0f),
                              HMM_AngleDeg(delta_time * CUBE_ROT_SPEED_DEG_PER_S * 0.7f)));
    g_game.cube_orientation = HMM_MulQ(g_game.cube_orientation, cube_rotation);

    view_projection_t vp = {
        .view = HMM_M4D(1.0f),
        .proj = HMM_Perspective_RH_NO(HMM_AngleDeg(60.0f),
                                      (f32)extent.width / (f32)extent.height, 0.1f, 100.0f),
    };
    Renderer_SetBufferObject(g_game.vp_uniform, &vp, sizeof(vp));

    flat_push_constant_t cube_push_constant;
    cube_push_constant.transform = HMM_MulM4(HMM_Translate(V3(-1.5f, 0.0f, -3.0f)), HMM_QToM4(g_game.cube_orientation));
    cube_push_constant.color = V4(0.9f, 0.5f, 0.2f, 1.0f);
    Renderer_DrawMesh(SWAPCHAIN_PASS_HANDLE, g_game.cube_pipeline, &cube_push_constant,
                      g_game.cube_mesh);

    cube_push_constant.transform = HMM_MulM4(HMM_Translate(V3(1.5f, 0.0f, -3.0f)), HMM_QToM4(g_game.cube_orientation));
    cube_push_constant.color = V4(0.5f, 0.9f, 0.2f, 1.0f);
    Renderer_DrawMesh(SWAPCHAIN_PASS_HANDLE, g_game.cube_pipeline, &cube_push_constant,
                      g_game.cube_mesh);

    /* textured quad */
    textured_push_constant_t quad_push_constant;
    quad_push_constant.transform = HMM_MulM4(HMM_Translate(V3(200.0f, 500.0f, 0.0f)), HMM_Scale(V3(200.0f, 200.0f, 1.0f)));
    quad_push_constant.color = V4(1.0f, 1.0f, 1.0f, 1.0f);
    quad_push_constant.texture = g_game.quad_texture;
    Renderer_DrawMesh(SWAPCHAIN_PASS_HANDLE, g_game.quad_pipeline, &quad_push_constant,
                      g_game.quad_mesh);

    quad_push_constant.transform = HMM_MulM4(HMM_Translate(V3(100.0f, 100.0f, 0.0f)), HMM_Scale(V3(100.0f,100.0f,1.0f)));
    quad_push_constant.color = V4(1.0f, 0.0f, 0.0f, 1.0f);
    quad_push_constant.texture = g_game.font_texture;
    Renderer_DrawMesh(SWAPCHAIN_PASS_HANDLE, g_game.quad_pipeline, &quad_push_constant,
                      g_game.quad_mesh);

    /* instanced text */
    text_push_constant_t text_push_constant = {.texture = g_game.font_texture};
    Renderer_DrawMeshInstanced(SWAPCHAIN_PASS_HANDLE, g_game.text_pipeline, &text_push_constant,
                               g_game.text_instances, g_game.text_instance_count,
                               g_game.quad_mesh);

    Engine_EndFrame();
}
