#include "core.h"
#include "core_string.h"
#include "log.h"
#include "memory_arena.h"
#include "os_time.h"

#include "engine_main.h"
#include "mesh.h"
#include "platform.h"
#include "render_types.h"
#include "renderer.h"
#include "draw.h"
#include "vulkan_renderer.h"
#include "console.h"

#define MAX_FRAMETIME_SAMPLES   512
#define SAMPLE_WINDOW_S         0.2f

typedef struct
{
    arena_t *arena;


    // stats
    f32 frametime_samples[MAX_FRAMETIME_SAMPLES];
    u32 frametime_sample_i;
    f32 frametime_sum;
    u32 frametime_sum_count;

    f32 avg_frametime;
    u32 fps;

} engine_t;


static engine_t s_engine = {};

static void draw_version_label();
static void draw_stats();

bool Engine_Init(platform_window_t *window)
{
    s_engine.arena = MemoryArena_Create("engine-arena");

    if (!VulkanRenderer_Init(s_engine.arena, window))
        goto fail;

    if (!Renderer_Init(s_engine.arena))
        goto fail_renderer;

    if (!MeshManager_Init(s_engine.arena))
        goto fail_renderer;

    if (!Draw_Init())
        goto fail_renderer;

    if (!Console_Init())
        goto fail_renderer;

    return true;

fail_renderer:
    VulkanRenderer_Destroy();
fail:
    MemoryArena_Destroy(s_engine.arena);
    s_engine.arena = NULL;
    return false;
}

void Engine_Destroy(void)
{
    Draw_Destroy();
    VulkanRenderer_Destroy();

    MemoryArena_Print(s_engine.arena);
    MemoryArena_Destroy(s_engine.arena);
    s_engine.arena = NULL;
}

void Engine_HandleResize(u32 width, u32 height)
{
    Log(DEBUG, "window resized to %ux%u", width, height);

    VulkanRenderer_HandleResize(width, height);

    Draw_HandleResize(width, height);
}

key_handle_result_t Engine_HandleKeyDown(key_code_t key)
{
    if (Console_HandleKeyDown(key) == KEY_EVENT_CONSUMED)
        return KEY_EVENT_CONSUMED;

    return KEY_EVENT_PASSTHROUGH;
}

key_handle_result_t Engine_HandleKeyUp(key_code_t key)
{
    if (Console_HandleKeyUp(key) == KEY_EVENT_CONSUMED)
        return KEY_EVENT_CONSUMED;

    return KEY_EVENT_PASSTHROUGH;
}

f32 Engine_BeginFrame(void)
{
    engine_t *engine = &s_engine;

    static u64 last_time_ns;

    u64 now_ns = OS_TimeNowNs();
    if (last_time_ns == 0)
        last_time_ns = now_ns;

    f32 delta_time = (f32)(now_ns - last_time_ns) / (f32)NS_PER_SECOND;
    last_time_ns = now_ns;

    // Stats
    engine->frametime_samples[engine->frametime_sample_i++] = delta_time;
    if (engine->frametime_sample_i == MAX_FRAMETIME_SAMPLES)
        engine->frametime_sample_i = 0;

    engine->frametime_sum += delta_time;
    engine->frametime_sum_count++;
    if (engine->frametime_sum > SAMPLE_WINDOW_S)
    {
        engine->avg_frametime = engine->frametime_sum / engine->frametime_sum_count;
        engine->fps = (u32)(1.0f / (engine->avg_frametime));

        engine->frametime_sum = 0.0f;
        engine->frametime_sum_count = 0;
    }

    VulkanRenderer_BeginFrame(); // Needs to be first
    Draw_BeginFrame();

    Console_Update(delta_time);

    return delta_time;
}

void Engine_EndFrame(void)
{
    draw_version_label();
    draw_stats();

    Console_Draw();

    Draw_EndFrame();

    VulkanRenderer_EndFrame(); // Needs to be last
}

static void draw_version_label()
{
    window_extent_t extent = Renderer_GetWindowExtent();
    Draw_SetTextSize(16);
    Draw_SetTextColor(V4(1.0, 1.0, 1.0, 1.0));
    Draw_Text(extent.width - 168, extent.height - 32, string_lit("DCFS 0.0.1"));
}

static void draw_stats()
{
    window_extent_t extent = Renderer_GetWindowExtent();
    scratch_t scratch = Scratch_Begin(s_engine.arena);

    Draw_SetTextSize(16);
    Draw_SetTextColor(V4(1.0, 1.0, 1.0, 1.0));


    string fps_s = string_fmt(scratch.arena, "FPS: %u", s_engine.fps);
    string frametime_s = string_fmt(scratch.arena, "Frametime: %.3f ms", (s_engine.avg_frametime * 1000.0f));

    Draw_Text(8, extent.height - 32, fps_s);
    Draw_Text(8, extent.height - 64, frametime_s);

    Scratch_End(scratch);
}
