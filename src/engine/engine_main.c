#include <SDL3/SDL_timer.h>

#include "core.h"

#include "engine_main.h"
#include "game_main.h"

void Engine_HandleKeyDown(SDL_Keycode key)
{
    Game_HandleKeyDown(key);
}

void Engine_HandleKeyUp(SDL_Keycode key)
{
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

    Game_Tick(delta_time);
}
