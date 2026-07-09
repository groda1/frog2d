#include <SDL3/SDL_video.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_error.h>

#include "log.h"
#include "game_main.h"


#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080

const char *__lsan_default_suppressions(void);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    Log_Init();

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

    if (!Game_Init(window))
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

                Game_HandleKeyDown(event.key.key);
            }
            else if (event.type == SDL_EVENT_KEY_UP)
            {
                Game_HandleKeyUp(event.key.key);
            }
            else if (event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                Game_HandleResize((u32)event.window.data1, (u32)event.window.data2);
            }
        }

        Game_Tick();
    }

    Game_Destroy();

exit:
    SDL_DestroyWindow(window);
    SDL_Quit();

    Log_Destroy();

    return 0;
}

const char *__lsan_default_suppressions(void)
{
    return "leak:libdbus-1\n";
}
