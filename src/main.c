#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_error.h>

#include "core.h"
#include "log.h"
#include "memory_arena.h"

static bool frog2d_sdl_init()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
        return false;
    return true;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    arena_t *main_arena = MemoryArena_Create("main-arena");

    MemoryArena_Print(main_arena);

    Log_Init(main_arena);

    if (!frog2d_sdl_init())
    {
        printf("Failed to init SDL: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_Window *window = SDL_CreateWindow("frog2d test", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        printf("Failed to create window: %s\n", SDL_GetError());

        SDL_Quit();
        return EXIT_FAILURE;
    }

    //SDL_Vulkan_CreateSurface()

    SDL_Event event;
    bool m_running = true;
    while (m_running)
    {
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
            }
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    MemoryArena_Print(main_arena);

    MemoryArena_Destroy(main_arena);

    return EXIT_SUCCESS;
}
