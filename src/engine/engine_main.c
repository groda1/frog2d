#include <SDL3/SDL_timer.h>

#include "core.h"
#include "log.h"
#include "memory_arena.h"

#include "engine_main.h"
#include "mesh.h"
#include "renderer.h"
#include "text.h"
#include "vulkan_renderer.h"

static arena_t *g_engine_arena;

bool Engine_Init(SDL_Window *window)
{
    g_engine_arena = MemoryArena_Create("engine-arena");

    if (!VulkanRenderer_Init(g_engine_arena, window))
        goto fail;

    if (!Renderer_Init(g_engine_arena))
        goto fail_renderer;

    if (!MeshManager_Init(g_engine_arena))
        goto fail_renderer;

    if (!Text_Init(g_engine_arena, 8192))
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
    Text_Destroy();

    MemoryArena_Print(g_engine_arena);
    MemoryArena_Destroy(g_engine_arena);
    g_engine_arena = NULL;
}

void Engine_HandleResize(u32 width, u32 height)
{
    Log(DEBUG, "window resized to %ux%u", width, height);

    VulkanRenderer_HandleResize(width, height);

    Text_HandleResize(width, height);
}

f32 Engine_BeginFrame(void)
{
    static u64 last_time_ns;

    u64 now_ns = SDL_GetTicksNS();
    if (last_time_ns == 0)
        last_time_ns = now_ns;

    f32 delta_time = (f32)(now_ns - last_time_ns) / (f32)SDL_NS_PER_SECOND;
    last_time_ns = now_ns;

    VulkanRenderer_BeginFrame();

    Text_BeginFrame();

    return delta_time;
}

void Engine_EndFrame(void)
{
    Text_EndFrame();

    // Needs to be last
    VulkanRenderer_EndFrame();
}
