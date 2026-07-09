#include <SDL3/SDL_video.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_error.h>

#include "memory_arena.h"

#include "log.h"
#include "vulkan_renderer.h"
#include "engine_main.h"


#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080


int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    arena_t *main_arena = MemoryArena_Create("main-arena");

    Log_Init(main_arena);

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        Log(ERROR, "Failed to init SDL: %s\n", SDL_GetError());
        return -1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "dungeon crawl frog soup", DEFAULT_WIDTH, DEFAULT_HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        Log(ERROR, "Failed to create window: %s\n", SDL_GetError());

        SDL_Quit();
        return -1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    if (!VulkanRenderer_Init(main_arena, window))
        goto exit;

    SDL_ShowWindow(window);

    bool m_running = true;
    while (m_running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                m_running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                if (event.key.key == SDLK_ESCAPE)
                {
                    m_running = false;
                }

                Engine_HandleKeyDown(event.key.key);
            }
            else if (event.type == SDL_EVENT_KEY_UP)
            {
                Engine_HandleKeyUp(event.key.key);
            }
        }

        Engine_Tick();
    }

    VulkanRenderer_Destroy();

exit:
    SDL_DestroyWindow(window);
    SDL_Quit();

    MemoryArena_Print(main_arena);
    MemoryArena_Destroy(main_arena);

    return 0;
}
