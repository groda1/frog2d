#include <SDL3/SDL_timer.h>

#include "core.h"
#include "log.h"
#include "memory_arena.h"

#include "engine_main.h"
#include "game_main.h"
#include "vulkan_renderer.h"

static arena_t *g_engine_arena;

bool Engine_Init(SDL_Window *window)
{
    g_engine_arena = MemoryArena_Create("engine-arena");

    if (!VulkanRenderer_Init(g_engine_arena, window))
    {
        MemoryArena_Destroy(g_engine_arena);
        g_engine_arena = NULL;
        return false;
    }

    return true;
}

void Engine_Destroy(void)
{
    VulkanRenderer_Destroy();

    MemoryArena_Print(g_engine_arena);
    MemoryArena_Destroy(g_engine_arena);
    g_engine_arena = NULL;
}

void Engine_HandleKeyDown(SDL_Keycode key)
{
    Log(DEBUG, "key down %ju", key);
    Game_HandleKeyDown(key);
}

void Engine_HandleKeyUp(SDL_Keycode key)
{
    Log(DEBUG, "key up %ju", key);
    Game_HandleKeyUp(key);
}

void Engine_Tick(void)
{
    static u64 last_time_ns;

    u64 now_ns = SDL_GetTicksNS();
    if (last_time_ns == 0)
        last_time_ns = now_ns;

    f32 delta_time = (f32)(now_ns - last_time_ns) / (f32)SDL_NS_PER_SECOND;
    last_time_ns = now_ns;

    VulkanRenderer_BeginFrame();

    Game_Tick(delta_time);

    VulkanRenderer_EndFrame();
}
