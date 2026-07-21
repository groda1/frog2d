#include "HandmadeMath.h"
#include "core.h"
#include "core_math.h"
#include "log.h"

#include "engine_main.h"
#include "engine_types.h"
#include "game_main.h"
#include "mesh.h"
#include "platform.h"
#include "render_types.h"
#include "renderer.h"

/* milestone 0 sandbox: a grid and a movable cube, for tuning camera and feel */

#define GRID_WIDTH              24
#define GRID_HEIGHT             16
#define TILE_GAP                0.0f
#define TILE_THICKNESS          0.3f
#define TILE_COLOR_A            V4(0.30f, 0.32f, 0.28f, 1.0f)
#define TILE_COLOR_B            V4(0.23f, 0.25f, 0.22f, 1.0f)

#define PLAYER_SIZE             0.7f
#define PLAYER_COLOR            V4(0.9f, 0.5f, 0.2f, 1.0f)
#define PLAYER_MOVE_SPEED       7.0f;
#define PLAYER_BUMP_SPEED       5.0f
#define PLAYER_BUMP_DISTANCE    0.3f

#define CAMERA_PITCH_DEG        55.0f
#define CAMERA_DISTANCE         9.0f
#define CAMERA_FOV_DEG          60.0f
#define CAMERA_FOLLOW_SPEED     8.0f
#define CAMERA_BOT_CLAMP -2.0f
#define CAMERA_TOP_CLAMP 5.0f
#define CAMERA_RIGHT_CLAMP 7.0f
#define CAMERA_LEFT_CLAMP 7.0f

typedef struct
{
    mat4 transform;
    vec4 color;
} flat_push_constant_t;

typedef enum
{
    PLAYER_ANIM_NONE,
    PLAYER_ANIM_MOVE,
    PLAYER_ANIM_BUMP,
} player_anim_t;

typedef struct
{
    mesh_handle_t cube_mesh;
    mesh_handle_t player_mesh;
    pipeline_handle_t tile_pipeline;
    pipeline_handle_t player_pipeline;
    buffer_object_handle_t vp_uniform;

    i32 player_pos_x;
    i32 player_pos_y;
    i32 player_target_pos_x;
    i32 player_target_pos_y;

    player_anim_t player_anim;
    f32 player_anim_progress;

    vec3 player_gfx_pos;
    vec3 player_gfx_target_pos;

    f32  player_gfx_old_rot;
    f32  player_gfx_rot;
    f32  player_gfx_target_rot;

    vec3 camera_target;     /* smoothed follow point */

} game_t;

static game_t g_game;

static void update_player(f32 delta_time);
static void update_player_move(f32 delta_time);
static void update_player_bump(f32 delta_time);
static void update_camera(f32 delta_time);
static void draw_grid(void);
static void draw_player(void);
static vec3 tile_center(i32 x, i32 y);
static vec3 player_center(i32 x, i32 y);
static f32  wrap_angle_deg(f32 angle);
static bool player_attempt_move(i32 new_x, i32 new_y);

bool Game_Init(platform_window_t *window)
{
    if (!Engine_Init(window))
        return false;

    g_game.cube_mesh = MeshManager_GetPredefinedMesh(PREDEFINED_MESH_NORMALED_CUBE);

    g_game.vp_uniform = Renderer_CreateUniformBuffer(sizeof(view_projection_t),
                                                     UNIFORM_STAGE_VERTEX);
    if (g_game.vp_uniform == BUFFER_OBJECT_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create view projection uniform");
        Engine_Destroy();
        return false;
    }

    pipeline_config_t tile_pipeline_config = {
        .name = "grid-tile",
        .vertex_shader = Renderer_LoadShader("shaders/flat_color.vert.spv"),
        .fragment_shader = Renderer_LoadShader("shaders/flat_color.frag.spv"),
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

    g_game.tile_pipeline = Renderer_AddPipeline(SWAPCHAIN_PASS_HANDLE, &tile_pipeline_config);
    if (g_game.tile_pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create tile pipeline");
        Engine_Destroy();
        return false;
    }

    pipeline_config_t player_pipeline_config = {
        .name = "player",
        .vertex_shader = Renderer_LoadShader("shaders/player.vert.spv"),
        .fragment_shader = Renderer_LoadShader("shaders/player.frag.spv"),
        .push_constant_size = sizeof(flat_push_constant_t),
        .vertex_stride = sizeof(normal_vertex_t),
        .vertex_attribute_count = 2,
        .vertex_attributes = {
            {
                .location = 0,
                .format = VERTEX_FORMAT_F32X3,
                .offset = offsetof(normal_vertex_t, position),
            },
            {
                .location = 1,
                .format = VERTEX_FORMAT_F32X3,
                .offset = offsetof(normal_vertex_t, normal),
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

    g_game.player_pipeline = Renderer_AddPipeline(SWAPCHAIN_PASS_HANDLE, &player_pipeline_config);
    if (g_game.player_pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create player pipeline");
        Engine_Destroy();
        return false;
    }

    g_game.player_mesh = MeshManager_LoadMesh(string_lit("resources/models/suzanne.obj"));

    g_game.player_pos_x = GRID_WIDTH / 2;
    g_game.player_pos_y = GRID_HEIGHT / 2;

    g_game.player_gfx_pos = player_center(g_game.player_pos_x, g_game.player_pos_y);
    g_game.player_gfx_target_pos = g_game.player_gfx_pos;
    g_game.player_gfx_rot = 0.0f;
    g_game.player_gfx_target_rot = g_game.player_gfx_rot;
    g_game.camera_target = g_game.player_gfx_pos;

    return true;
}

void Game_Destroy(void)
{
    Engine_Destroy();
}

void Game_HandleKeyDown(key_code_t key)
{
    game_t *game = &g_game;

    if (Engine_HandleKeyDown(key) == KEY_EVENT_CONSUMED)
        return;

    if (game->player_anim == PLAYER_ANIM_NONE)
    {
        switch (key)
        {
            case KEY_D:
            case KEY_RIGHT:
            {
                player_attempt_move(game->player_pos_x + 1, game->player_pos_y);
                break;
            }
            case KEY_A:
            case KEY_LEFT:
            {
                player_attempt_move(game->player_pos_x - 1, game->player_pos_y);
                break;
            }
            case KEY_W:
            case KEY_UP:
            {
                player_attempt_move(game->player_pos_x, game->player_pos_y + 1);
                break;
            }
            case KEY_S:
            case KEY_DOWN:
            {
                player_attempt_move(game->player_pos_x, game->player_pos_y - 1);
                break;
            }
            case KEY_KP_7:
            case KEY_HOME:
            {
                player_attempt_move(game->player_pos_x - 1, game->player_pos_y + 1);
                break;
            }
            case KEY_KP_1:
            case KEY_END:
            {
                player_attempt_move(game->player_pos_x - 1, game->player_pos_y - 1);
                break;
            }
            case KEY_KP_9:
            case KEY_PGUP:
            {
                player_attempt_move(game->player_pos_x + 1, game->player_pos_y + 1);
                break;
            }
            case KEY_KP_3:
            case KEY_PGDN:
            {
                player_attempt_move(game->player_pos_x + 1, game->player_pos_y - 1);
                break;
            }

            default:
                break;
        }
    }
}

void Game_HandleKeyUp(key_code_t key)
{
    Engine_HandleKeyUp(key);
}

void Game_HandleResize(u32 width, u32 height)
{
    Engine_HandleResize(width, height);
}

void Game_Tick(void)
{
    f32 delta_time = Engine_BeginFrame();

    update_player(delta_time);
    update_camera(delta_time);

    draw_grid();
    draw_player();

    Engine_EndFrame();
}

static void update_player(f32 delta_time)
{
    switch (g_game.player_anim)
    {
        case PLAYER_ANIM_MOVE:
            update_player_move(delta_time);
            break;
        case PLAYER_ANIM_BUMP:
            update_player_bump(delta_time);
            break;
        case PLAYER_ANIM_NONE:
            break;
    }
}

static void update_player_move(f32 delta_time)
{
    game_t *game = &g_game;

    game->player_anim_progress += delta_time * PLAYER_MOVE_SPEED;

    f32 t = game->player_anim_progress;
    if (t > 1.0f)
        t = 1.0f;

    vec3 old = player_center(game->player_pos_x, game->player_pos_y);
    game->player_gfx_pos = lerp(old, smoothstep(t), game->player_gfx_target_pos);
    game->player_gfx_rot = lerp(game->player_gfx_old_rot, smoothstep(t), game->player_gfx_target_rot);

    if (game->player_anim_progress >= 1.0f)
    {
        game->player_pos_x = game->player_target_pos_x;
        game->player_pos_y = game->player_target_pos_y;
        game->player_anim = PLAYER_ANIM_NONE;
        Log(DEBUG, "move complete %d,%d", game->player_pos_x, game->player_pos_y);
    }
}

static void update_player_bump(f32 delta_time)
{
    game_t *game = &g_game;

    game->player_anim_progress += delta_time * PLAYER_BUMP_SPEED;

    f32 t = game->player_anim_progress;
    if (t > 1.0f)
        t = 1.0f;

    vec3 old = player_center(game->player_pos_x, game->player_pos_y);
    game->player_gfx_pos = lerp(old, smootherstep(outbackstep(t)) * PLAYER_BUMP_DISTANCE,
                                      game->player_gfx_target_pos);
    game->player_gfx_rot = lerp(game->player_gfx_old_rot, smootherstep(outbackstep(t)), game->player_gfx_target_rot);

    if (game->player_anim_progress >= 1.0f)
    {
        game->player_gfx_pos = old;
        game->player_anim = PLAYER_ANIM_NONE;
    }
}

static void update_camera(f32 delta_time)
{
    game_t *game = &g_game;
    window_extent_t extent = Renderer_GetWindowExtent();

    f32 follow = CAMERA_FOLLOW_SPEED * delta_time;
    if (follow > 1.0f)
        follow = 1.0f;

    vec3 new_target = {
        .X = Clamp(CAMERA_LEFT_CLAMP, game->player_gfx_pos.X, GRID_WIDTH - CAMERA_RIGHT_CLAMP),
        .Y = game->player_gfx_pos.Y,
        .Z = Clamp(-GRID_HEIGHT + CAMERA_TOP_CLAMP, game->player_gfx_pos.Z, CAMERA_BOT_CLAMP),
    };

    game->camera_target = lerp(game->camera_target, follow, new_target);

    f32 pitch = HMM_AngleDeg(CAMERA_PITCH_DEG + 10);
    vec3 offset = V3(0.0f,
                     HMM_SinF(pitch) * CAMERA_DISTANCE,
                     HMM_CosF(pitch) * CAMERA_DISTANCE);
    vec3 eye = HMM_AddV3(game->camera_target, offset);

    view_projection_t vp = {
        .view = HMM_LookAt_RH(eye, game->camera_target, V3(0.0f, 1.0f, 0.0f)),
        .proj = HMM_Perspective_RH_NO(HMM_AngleDeg(CAMERA_FOV_DEG),
                                      (f32)extent.width / (f32)extent.height,
                                      0.1f, 100.0f),
    };
    Renderer_SetBufferObject(g_game.vp_uniform, &vp, sizeof(vp));
}

static void draw_grid(void)
{
    flat_push_constant_t push_constant;
    mat4 scale = HMM_Scale(V3(1.0f - TILE_GAP, TILE_THICKNESS, 1.0f - TILE_GAP));

    for (i32 z = 0; z < GRID_HEIGHT; z++)
    {
        for (i32 x = 0; x < GRID_WIDTH; x++)
        {
            vec3 center = tile_center(x, z);

            push_constant.transform = HMM_MulM4(HMM_Translate(center), scale);
            push_constant.color = ((x + z) & 1) ? TILE_COLOR_A : TILE_COLOR_B;
            Renderer_DrawMesh(SWAPCHAIN_PASS_HANDLE, g_game.tile_pipeline,
                              &push_constant, g_game.cube_mesh);
        }
    }
}

static void draw_player(void)
{
    flat_push_constant_t push_constant = {
        .transform = HMM_MulM4(
                        HMM_Translate( g_game.player_gfx_pos),
                        HMM_MulM4(
                            HMM_MulM4(
                                HMM_Rotate_RH(HMM_AngleDeg(-25), V3(1.0f, 0.0f, 0.0f)),
                                HMM_Rotate_RH(HMM_AngleDeg(g_game.player_gfx_rot), V3(0.0f, 1.0f, 0.0f))),
                            HMM_Scale(V3(PLAYER_SIZE, PLAYER_SIZE, PLAYER_SIZE))
                        )
                    ),
        .color = PLAYER_COLOR,
    };
    Renderer_DrawMesh(SWAPCHAIN_PASS_HANDLE, g_game.player_pipeline,
                      &push_constant, g_game.player_mesh);
}


static vec3 tile_center(i32 x, i32 y)
{
    return V3((f32)x + 0.5f, -TILE_THICKNESS / 2.0f, (f32)-y + 0.5f);
}

static vec3 player_center(i32 x, i32 y)
{
    return V3((f32)x + 0.5f, PLAYER_SIZE / 2.0f, (f32)-y + 0.5f);
}

static inline f32 wrap_angle_deg(f32 angle)
{
    while (angle > 180.0f)
        angle -= 360.0f;
    while (angle < -180.0f)
        angle += 360.0f;
    return angle;
}

static bool player_attempt_move(i32 new_x, i32 new_y)
{
    game_t *game = &g_game;

    Assert(game->player_anim == PLAYER_ANIM_NONE);

    game->player_anim_progress = 0.0f;
    game->player_gfx_target_pos = player_center(new_x, new_y);

    f32 rotation = game->player_gfx_rot;

    if (new_x > game->player_pos_x && new_y > game->player_pos_y ) // northeast
        rotation = 135.0f;
    else if (new_x < game->player_pos_x && new_y > game->player_pos_y) // northwest
        rotation = -135.0f;
    else if (new_x > game->player_pos_x && new_y < game->player_pos_y ) // southeast
        rotation = 45.0f;
    else if (new_x < game->player_pos_x && new_y < game->player_pos_y ) // southwest
        rotation = -45.0f;
    else if (new_x > game->player_pos_x) // east
        rotation = 90.0f;
    else if (new_x < game->player_pos_x) // west
        rotation = -90.0f;
    else if (new_y > game->player_pos_y) // north
        rotation = 180.0f;
    else if (new_y < game->player_pos_y) // south
        rotation = 0.0f;

    game->player_gfx_old_rot = wrap_angle_deg(game->player_gfx_rot);
    game->player_gfx_target_rot = game->player_gfx_old_rot
                                + wrap_angle_deg(rotation - game->player_gfx_old_rot);

    if (new_x >= 0 && new_x < GRID_WIDTH && new_y >= 0 && new_y < GRID_HEIGHT)
    {
        game->player_target_pos_x = new_x;
        game->player_target_pos_y = new_y;
        game->player_anim = PLAYER_ANIM_MOVE;
        return true;
    }

    game->player_anim = PLAYER_ANIM_BUMP;
    return false;
}
