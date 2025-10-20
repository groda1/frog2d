#include <SDL3/SDL_video.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_error.h>

#include "core.h"
#include "core_string.h"
#include "memory_arena.h"

#include "log.h"
#include "vulkan_renderer.h"


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
        return EXIT_FAILURE;
    }

    SDL_Window *window = SDL_CreateWindow(
        "frog2d test", DEFAULT_WIDTH, DEFAULT_HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        Log(ERROR, "Failed to create window: %s\n", SDL_GetError());

        SDL_Quit();
        return EXIT_FAILURE;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    if (!VulkanRenderer_Init(main_arena, window))
        goto _exit;

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
            }
        }
    }

    VulkanRenderer_Destroy();
_exit:
    SDL_DestroyWindow(window);
    SDL_Quit();

    MemoryArena_Print(main_arena);
    MemoryArena_Destroy(main_arena);

    return EXIT_SUCCESS;
}
