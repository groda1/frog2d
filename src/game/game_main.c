#include "HandmadeMath.h"
#include "core.h"
#include "core_math.h"
#include "core_string.h"
#include "log.h"

#include "engine_main.h"
#include "engine_types.h"
#include "game_main.h"
#include "mesh.h"
#include "render_types.h"
#include "renderer.h"
#include "draw.h"

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

    /* right cube rendered into a low-res image target, shown on a quad */
    texture_handle_t cube_render_texture;
    renderpass_handle_t cube_pass;
    pipeline_handle_t cube_offscreen_pipeline;
    buffer_object_handle_t vp_uniform_cube_pass;

} game_t;

static game_t g_game;

bool Game_Init(platform_window_t *window)
{
    if (!Engine_Init(window))
        return false;

    g_game.mesh = MeshManager_GetPredefinedMesh(PREDEFINED_MESH_COLORED_TRIANGLE);

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
    g_game.cube_mesh = MeshManager_GetPredefinedMesh(PREDEFINED_MESH_NORMALED_CUBE);
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
    g_game.quad_mesh = MeshManager_GetPredefinedMesh(PREDEFINED_MESH_TEXTURED_QUAD);

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
        .alpha_blending = true,
    };

    g_game.quad_pipeline = Renderer_AddPipeline(SWAPCHAIN_PASS_HANDLE, &quad_pipeline_config);
    if (g_game.quad_pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create flat textured pipeline");
        Engine_Destroy();
        return false;
    }

    /* offscreen cube: a 128x128 image target displayed on a 512x512 quad */
    g_game.cube_render_texture = Renderer_CreateRenderTexture(128, 128, sampler);
    if (g_game.cube_render_texture == TEXTURE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create cube render texture");
        Engine_Destroy();
        return false;
    }

    g_game.cube_pass = Renderer_CreateRenderPass(g_game.cube_render_texture, 0);
    if (g_game.cube_pass == RENDERPASS_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create cube render pass");
        Engine_Destroy();
        return false;
    }

    g_game.vp_uniform_cube_pass = Renderer_CreateUniformBuffer(sizeof(view_projection_t),
                                                               UNIFORM_STAGE_VERTEX);
    if (g_game.vp_uniform_cube_pass == BUFFER_OBJECT_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create cube pass view projection uniform");
        Engine_Destroy();
        return false;
    }

    view_projection_t cube_pass_vp = {
        .view = HMM_M4D(1.0f),
        .proj = HMM_Perspective_RH_NO(HMM_AngleDeg(60.0f), 1.0f, 0.1f, 100.0f),
    };
    Renderer_SetBufferObject(g_game.vp_uniform_cube_pass, &cube_pass_vp, sizeof(cube_pass_vp));

    pipeline_config_t cube_offscreen_config = cube_pipeline_config;
    cube_offscreen_config.uniform_bindings[0].buffer_object = g_game.vp_uniform_cube_pass;

    g_game.cube_offscreen_pipeline = Renderer_AddPipeline(g_game.cube_pass,
                                                          &cube_offscreen_config);
    if (g_game.cube_offscreen_pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create offscreen cube pipeline");
        Engine_Destroy();
        return false;
    }

    view_projection_t vp2 = {
        .view = HMM_M4D(1.0f),
        .proj = HMM_Orthographic_RH_NO(0.0f, (f32)extent.width, 0.0f, (f32)extent.height, -1.0, 1.0)
    };
    Renderer_SetBufferObject(g_game.vp_uniform_ortho, &vp2, sizeof(vp2));

    Log(ERROR, "test error");
    Log(WARNING, "test warn");

    return true;
}

void Game_Destroy(void)
{
    Engine_Destroy();
}

void Game_HandleKeyDown(key_code_t key)
{
    Log(DEBUG, "handle key: %u", key);
    Engine_HandleKeyDown(key);
}

void Game_HandleKeyUp(key_code_t key)
{
    Engine_HandleKeyUp(key);
}

void Game_HandleResize(u32 width, u32 height)
{
    Engine_HandleResize(width, height);

    view_projection_t vp2 = {
        .view = HMM_M4D(1.0f),
        .proj = HMM_Orthographic_RH_NO(0.0f, (f32)width, 0.0f, (f32)height, -1.0, 1.0)
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
    g_game.cube_orientation =
        HMM_MulQ(g_game.cube_orientation,
                 HMM_QFromAxisAngle_RH(V3(0.0f, 1.0f, 0.0f), HMM_AngleDeg(-delta_time * CUBE_ROT_SPEED_DEG_PER_S * 0.5)));
    g_game.cube_orientation =
        HMM_MulQ(g_game.cube_orientation,
                HMM_QFromAxisAngle_RH(V3(1.0f, 0.0f, 0.0f), HMM_AngleDeg(delta_time * CUBE_ROT_SPEED_DEG_PER_S * 0.5)));

    //g_game.cube_orientation = HMM_MulQ(g_game.cube_orientation, cube_rotation);

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

    /* right cube: rendered offscreen into the cube pass, centered in its view */
    cube_push_constant.transform = HMM_MulM4(HMM_Translate(V3(0.0f, 0.0f, -2.0f)), HMM_QToM4(g_game.cube_orientation));
    cube_push_constant.color = V4(0.5f, 0.9f, 0.2f, 1.0f);
    Renderer_DrawMesh(g_game.cube_pass, g_game.cube_offscreen_pipeline, &cube_push_constant,
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

    /* the offscreen cube's 128x128 target upscaled onto a 512x512 quad */
    quad_push_constant.transform = HMM_MulM4(HMM_Translate(V3((f32)extent.width - 400.0f, 408.0f, 0.0f)), HMM_Scale(V3(1024.0f, 1024.0f, 1.0f)));
    quad_push_constant.color = V4(1.0f, 1.0f, 1.0f, 1.0f);
    quad_push_constant.texture = g_game.cube_render_texture;
    Renderer_DrawMesh(SWAPCHAIN_PASS_HANDLE, g_game.quad_pipeline, &quad_push_constant,
                      g_game.quad_mesh);

    Engine_EndFrame();
}
