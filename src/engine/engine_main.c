#include "core.h"
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

static arena_t *g_engine_arena;

static void draw_version_label();

bool Engine_Init(platform_window_t *window)
{
    g_engine_arena = MemoryArena_Create("engine-arena");

    if (!VulkanRenderer_Init(g_engine_arena, window))
        goto fail;

    if (!Renderer_Init(g_engine_arena))
        goto fail_renderer;

    if (!MeshManager_Init(g_engine_arena))
        goto fail_renderer;

    if (!Draw_Init())
        goto fail_renderer;

    return true;

fail_renderer:
    VulkanRenderer_Destroy();
fail:
    MemoryArena_Destroy(g_engine_arena);
    g_engine_arena = NULL;
    return false;
}

void Engine_Destroy(void)
{
    VulkanRenderer_Destroy();
    Draw_Destroy();

    MemoryArena_Print(g_engine_arena);
    MemoryArena_Destroy(g_engine_arena);
    g_engine_arena = NULL;
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
    static u64 last_time_ns;

    u64 now_ns = OS_TimeNowNs();
    if (last_time_ns == 0)
        last_time_ns = now_ns;

    f32 delta_time = (f32)(now_ns - last_time_ns) / (f32)NS_PER_SECOND;
    last_time_ns = now_ns;

    VulkanRenderer_BeginFrame(); // Needs to be first
    Draw_BeginFrame();

    Console_Update(delta_time);

    draw_version_label();

    Console_Draw();



    return delta_time;
}

void Engine_EndFrame(void)
{
    Draw_EndFrame();

    VulkanRenderer_EndFrame(); // Needs to be last
}

static void draw_version_label()
{
    window_extent_t extent = Renderer_GetWindowExtent();
    Draw_SetTextSize(16);
    Draw_SetTextColor(V4(1.0, 1.0, 1.0, 1.0));
    Draw_Text(extent.width - 164, extent.height - 34, string_lit("DCFS 0.0.1"));
}
