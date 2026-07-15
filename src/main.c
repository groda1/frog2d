#include "log.h"
#include "platform.h"
#include "game_main.h"


#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    Log_Init();

    if (!Platform_Init())
        return -1;

    platform_window_t *window = Platform_CreateWindow("dungeon crawl frog soup",
                                                      DEFAULT_WIDTH, DEFAULT_HEIGHT);
    if (!window)
    {
        Platform_Shutdown();
        return -1;
    }

    if (!Game_Init(window))
        goto exit;

    Platform_ShowWindow(window);

    bool m_running = true;
    while (m_running)
    {
        platform_event_t event;
        while (Platform_PollEvent(&event))
        {
            switch (event.type)
            {
            case PLATFORM_EVENT_QUIT:
                m_running = false;
                break;

            case PLATFORM_EVENT_KEY_DOWN:
                if (event.key == KEY_ESCAPE)
                    m_running = false;

                Game_HandleKeyDown(event.key);
                break;

            case PLATFORM_EVENT_KEY_UP:
                Game_HandleKeyUp(event.key);
                break;

            case PLATFORM_EVENT_WINDOW_RESIZED:
                Game_HandleResize(event.resize.width, event.resize.height);
                break;
            }
        }

        Game_Tick();
    }

    Game_Destroy();

exit:
    Platform_DestroyWindow(window);
    Platform_Shutdown();

    Log_Destroy();

    return 0;
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, PSTR cmd_line, int show_cmd)
{
    (void)instance;
    (void)prev_instance;
    (void)cmd_line;
    (void)show_cmd;

    return main(0, NULL);
}
#endif
