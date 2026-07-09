#include "core.h"
#include "core_math.h"
#include "core_string.h"
#include "log.h"

#include "engine_main.h"
#include "game_main.h"
#include "mesh.h"
#include "renderer.h"


/* port of the vulkrap hello_krap example: a wobbling triangle */

#define ROT_SPEED_DEG_PER_S 25.0f
#define WOBBLE_SPEED        5.0f
#define TRIANGLE_SCALE      512.0f

typedef struct
{
    mat4 transform;
    f32 wobble;
} push_constant_t;

typedef struct
{
    mesh_t mesh;
    vec3 position;
    quat orientation;
    push_constant_t push_constant;
    pipeline_handle_t pipeline;
} game_t;

static game_t g_game;

bool Game_Init(SDL_Window *window)
{
    if (!Engine_Init(window))
        return false;

    g_game.mesh = *MeshManager_GetMesh(PREDEFINED_MESH_COLORED_TRIANGLE);

    window_extent_t extent = Renderer_GetWindowExtent();
    g_game.position = V3((f32)extent.width / 2.0f, (f32)extent.height / 2.0f, 0.0f);
    g_game.orientation = HMM_Q(0.0f, 0.0f, 0.0f, 1.0f);
    g_game.push_constant.transform = HMM_M4D(1.0f);
    g_game.push_constant.wobble = 0.0f;

    pipeline_config_t pipeline_config = {
        .vertex_shader_path = string_lit("shaders/hello_triangle.vert.spv"),
        .fragment_shader_path = string_lit("shaders/hello_triangle.frag.spv"),
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
}

void Game_Tick(void)
{
    f32 delta_time = Engine_BeginFrame();

    /* update */
    quat rotation = HMM_QFromAxisAngle_RH(V3(0.0f, 0.0f, 1.0f),
                                          HMM_AngleDeg(-delta_time * ROT_SPEED_DEG_PER_S));
    g_game.orientation = HMM_MulQ(g_game.orientation, rotation);
    g_game.push_constant.wobble += delta_time * WOBBLE_SPEED;

    // TODO the view/projection belongs in a uniform buffer, but descriptor
    // sets are not ported yet, so it is baked into the push constant transform
    window_extent_t extent = Renderer_GetWindowExtent();
    mat4 projection = HMM_Orthographic_RH_NO(0.0f, (f32)extent.width,
                                             0.0f, (f32)extent.height, -1.0f, 1.0f);
    mat4 model = HMM_MulM4(HMM_Translate(g_game.position),
                           HMM_MulM4(HMM_QToM4(g_game.orientation),
                                     HMM_Scale(V3(TRIANGLE_SCALE, TRIANGLE_SCALE, TRIANGLE_SCALE))));
    g_game.push_constant.transform = HMM_MulM4(projection, model);

    /* draw */
    Renderer_DrawMesh(SWAPCHAIN_PASS_HANDLE, g_game.pipeline, &g_game.push_constant,
                      &g_game.mesh);

    Engine_EndFrame();
}
